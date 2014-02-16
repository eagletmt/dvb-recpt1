#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <fstream>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <poll.h>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include "config.h"
#include "tuner.h"
#include "oneshot_timer.h"
namespace po = boost::program_options;

static const int DEFAULT_CHUNK_SIZE = 131072;
static const char *ADAPTERS_CONFIG_FILE_PATH = SYSCONFDIR "/recpt1/adapters.conf";
static const char *CHANNELS_CONFIG_FILE_PATH = SYSCONFDIR "/recpt1/channels.conf";

static void die(const char *msg) __attribute__((noreturn));
static void recorder(int duration, recpt1::tuner& tuner, const char *outfile);
static po::variables_map& parse_config_file(const char *path, po::variables_map& vmap);

struct chunk
{
  char *buf;
  int capacity;
  ssize_t offset;
  ssize_t size;
  chunk(int c) : buf(static_cast<char *>(std::malloc(c))), capacity(c), offset(0), size(0)
  {
    if (!buf) {
      die("malloc");
    }
  }
  ~chunk() { std::free(buf); }

  void shrink()
  {
    buf = static_cast<char *>(std::realloc(buf, size));
  }
};

int main(int argc, char *argv[])
{
  if (argc < 4) {
    std::cerr << "usage: " << argv[0] << " channel duration outfile" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  int channel, duration;
  try {
    channel = boost::lexical_cast<int>(argv[1]);
    duration = boost::lexical_cast<int>(argv[2]);
  } catch (const boost::bad_lexical_cast&) {
    std::cerr << "bad lexical_cast in argv!" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  const char *outfile = argv[3];

  po::variables_map vmap;
  parse_config_file(ADAPTERS_CONFIG_FILE_PATH, vmap);

  std::vector<std::string> adapters;
  if (vmap.count("isdbt.adapter")) {
    adapters = vmap["isdbt.adapter"].as<decltype(adapters)>();
  } else {
    std::cerr << "specify at least one adapter in " << ADAPTERS_CONFIG_FILE_PATH << " or program options" << std::endl;
    return 1;
  }

  std::ifstream ifs(CHANNELS_CONFIG_FILE_PATH);
  if (!ifs) {
    std::cerr << "cannot open " << CHANNELS_CONFIG_FILE_PATH << std::endl;
    return 1;
  }
  recpt1::tuner tuner(adapters, recpt1::tuner::parse_channels(ifs));
  if (tuner.tune(channel) && tuner.track()) {
    recorder(duration, tuner, outfile);
    return 0;
  } else {
    return 1;
  }
}

void die(const char *msg)
{
  std::perror(msg);
  std::exit(EXIT_FAILURE);
}

int create_master(const std::string& adapter, std::string& path, sockaddr_un& sun, socklen_t& len)
{
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    std::perror("socket");
    return -1;
  }
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100*1000;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1) {
    std::perror("setsockopt(SO_RCVTIMEO)");
    close(fd);
    return -1;
  }
  path = adapter;
  std::replace(path.begin(), path.end(), '/', '-');
  path = "/tmp/recpt1" + path + ".sock";
  sun.sun_family = AF_UNIX;
  std::snprintf(sun.sun_path, sizeof sun.sun_path, "%s", path.c_str());
  len = sizeof sun.sun_family + path.size();
  if (bind(fd, static_cast<struct sockaddr *>(static_cast<void *>(&sun)), len) == -1) {
    std::perror("bind");
    close(fd);
    return -1;
  }
  if (listen(fd, 5) == -1) {
    std::perror("listen");
    unlink(path.c_str());
    close(fd);
    return -1;
  }
  return fd;
}

void recorder(int duration, recpt1::tuner& tuner, const char *outfile)
{
  recpt1::oneshot_timer timer(duration);

  const std::string path = tuner.adapter() + "/dvr0";
  int rfd = open(path.c_str(), O_RDONLY);
  if (rfd == -1) {
    die("open(dev)");
  }

  std::atomic<bool> f_exit(false);

  struct sockaddr_un sun;
  socklen_t socklen;
  std::string sock_path;
  int listenfd = create_master(tuner.adapter(), sock_path, sun, socklen);
  std::thread master([&f_exit, &tuner, &timer, listenfd, &sun, &socklen]() {
    while (!f_exit) {
      int fd = accept(listenfd, static_cast<sockaddr *>(static_cast<void *>(&sun)), &socklen);
      if (fd != -1) {
        char buf[BUFSIZ];
        ssize_t n = read(fd, buf, sizeof buf - 1);
        if (n < 0) {
          std::perror("read(accept)");
        } else if (n > 0) {
          buf[n] = '\0';
          std::istringstream iss(buf);
          std::string cmd;
          if (iss >> cmd) {
            if (cmd == "tune") {
              int ch;
              if (iss >> ch) {
                std::cout << "Switch to channel " << ch << "..." << std::endl;
                if (tuner.tune(ch) && tuner.track()) {
                  std::cout << "Successfully tuned" << std::endl;
                } else {
                  std::cout << "Failed to tune!" << std::endl;
                  f_exit = true;
                }
              }
            } else if (cmd == "extend") {
              int t;
              if (iss >> t) {
                std::cout << "Extend time " << t << "..." << std::endl;
                timer.extend(t);
              }
            } else {
              std::cerr << "Unknown command " << cmd << std::endl;
            }
          }
        }
        close(fd);
      }
    }
  });

  int wfd = open(outfile, O_WRONLY | O_CREAT, 0644);
  if (wfd == -1) {
    die("open(outfile)");
  }

  std::queue<chunk *> q;
  std::mutex queue_mutex;

  std::thread writer([&f_exit, &q, &queue_mutex, wfd]() {
    while (!f_exit) {
      bool empty = true;
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (!q.empty()) {
          empty = false;
          chunk *c = q.front();
          const ssize_t w = write(wfd, c->buf + c->offset, c->size - c->offset);
          if (w < 0) {
            std::perror("write(outfile)");
            // discard this chunk
            delete c;
            q.pop();
          } else {
            c->offset += w;
            if (c->offset >= c->size) {
              delete c;
              q.pop();
            }
          }
        }
      }
      if (empty) {
        usleep(100 * 1000);
      }
    }
  });

  struct pollfd fds[2];
  fds[0].fd = timer.fd();
  fds[0].events = POLLIN | POLLERR;
  fds[1].fd = rfd;
  fds[1].events = POLLIN | POLLERR;
  for (;;) {
    for (struct pollfd& pfd : fds) {
      pfd.revents = 0;
    }
    int nfds = poll(fds, 2, 5*1000);
    if (nfds < 0) {
      std::perror("poll");
    } else if (nfds > 0) {
      if (fds[1].revents & POLLIN) {
        chunk *c = new chunk(DEFAULT_CHUNK_SIZE);
        c->size = read(rfd, c->buf, c->capacity);
        if (c->size < 0) {
          std::perror("read(dev)");
        } else {
          c->shrink();
          std::lock_guard<std::mutex> lock(queue_mutex);
          q.push(c);
        }
      }

      if (fds[0].revents & POLLIN) {
        f_exit = true;
        break;
      }
    }
  }
  close(rfd);
  writer.join();
  close(wfd);
  master.join();
  if (listenfd != -1) {
    unlink(sock_path.c_str());
  }
}

po::variables_map& parse_config_file(const char *path, po::variables_map& vmap)
{
  po::options_description desc("");
  desc.add_options()
    ("isdbt.adapter", po::value<std::vector<std::string> >(), "available adapters")
    ;

  std::ifstream ifs(path);
  if (ifs) {
    const po::parsed_options parsed = po::parse_config_file(ifs, desc, true);
    po::store(parsed, vmap);
    const std::vector<std::string> unknown = po::collect_unrecognized(parsed.options, po::include_positional);
    for (const std::string& name : unknown) {
      std::cerr << "warning: ignore unknown option " << name << std::endl;
    }
    po::notify(vmap);
  }
  return vmap;
}
