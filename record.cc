#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <queue>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include "tuner.h"
#include "oneshot_timer.h"
namespace po = boost::program_options;

static const int DEFAULT_CHUNK_SIZE = 131072;
static const char *ADAPTERS_CONFIG_FILE_PATH = "/etc/recpt1/adapters.conf";
static const char *CHANNELS_CONFIG_FILE_PATH = "/etc/recpt1/channels.conf";

static void die(const char *msg) __attribute__((noreturn));
static void recorder(int duration, const std::string& adapter, const char *outfile);
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
    recorder(duration, tuner.adapter(), outfile);
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

void recorder(int duration, const std::string& adapter, const char *outfile)
{
  recpt1::oneshot_timer timer(duration);

  const std::string path = adapter + "/dvr0";
  int rfd = open(path.c_str(), O_RDONLY);
  if (rfd == -1) {
    die("open(dev)");
  }

  int wfd = open(outfile, O_WRONLY | O_CREAT, 0644);
  if (wfd == -1) {
    die("open(outfile)");
  }

  struct pollfd fds[3];
  fds[0].fd = timer.fd();
  fds[0].events = POLLIN | POLLERR;
  fds[1].fd = rfd;
  fds[1].events = POLLIN | POLLERR;
  fds[2].fd = wfd;
  fds[2].events = POLLOUT | POLLERR;
  std::queue<chunk *> q;
  for (;;) {
    for (struct pollfd& pfd : fds) {
      pfd.revents = 0;
    }
    int nfds = poll(fds, 3, 5*1000);
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
          q.push(c);
        }
      }

      if (fds[2].revents & POLLOUT) {
        if (!q.empty()) {
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

      if (fds[0].revents & POLLIN) {
        break;
      }
    }
  }
  close(rfd);
  while (!q.empty()) {
    chunk *c = q.front();
    const ssize_t w = write(wfd, c, c->size);
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
  close(wfd);
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
