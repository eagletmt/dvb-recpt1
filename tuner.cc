#include "tuner.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/version.h>

namespace recpt1 {

tuner::tuner(const std::vector<std::string>& adapters, const std::unordered_map<int, channel>& channels)
 : frontend_fd_(-1), demux_fd_(-1), adapters_(adapters), adapter_(adapters.end()), channels_(channels)
{}

tuner::~tuner()
{
  if (frontend_fd_ != -1) {
    close(frontend_fd_);
  }
  if (demux_fd_ != -1) {
    close(demux_fd_);
  }
}

bool tuner::tune(int ch)
{
  if (frontend_fd_ != -1) {
    if (tune_impl(ch)) {
      return true;
    }
    close(frontend_fd_);
    frontend_fd_ = -1;
    if (demux_fd_ != -1) {
      close(demux_fd_);
      demux_fd_ = -1;
    }
  }

  for (auto it = adapters_.cbegin(); it != adapters_.cend(); ++it) {
    const std::string path = *it + "/frontend0";
    frontend_fd_ = open(path.c_str(), O_RDWR);
    if (frontend_fd_ == -1) {
      std::perror(("open(" + path + ")").c_str());
      continue;
    }
    adapter_ = it;
    if (tune_impl(ch)) {
      return true;
    }
    close(frontend_fd_);
    frontend_fd_ = -1;
    adapter_ = adapters_.cend();
  }
  return false;
}

bool tuner::tune_impl(int ch)
{
  DECLTYPE(channels_)::const_iterator jt = channels_.cend();

  struct dvb_frontend_info info;
  if (ioctl(frontend_fd_, FE_GET_INFO, &info) == -1) {
    std::perror("ioctl FE_GET_INFO");
    return false;
  }
  if (info.type == FE_OFDM) {
    jt = channels_.find(ch);
  }
  if (jt == channels_.cend()) {
    return false;
  }

  struct dtv_property prop[3];
  prop[0].cmd = DTV_FREQUENCY;
  prop[0].u.data = jt->second.frequency;
  prop[1].cmd = DTV_ISDBS_TS_ID;
  prop[1].u.data = jt->second.ts_id;
  prop[2].cmd = DTV_TUNE;

  struct dtv_properties props;
  props.props = prop;
  props.num = 3;
  if (ioctl(frontend_fd_, FE_SET_PROPERTY, &props) == -1) {
    std::perror("ioctl FE_SET_PROPERTY");
    return false;
  }

  fe_status_t status;
  for (int j = 0; j < 4; j++) {
    if (ioctl(frontend_fd_, FE_READ_STATUS, &status) == -1) {
      std::perror("ioctl FE_READ_STATUS");
    }
    if (status & FE_HAS_LOCK) {
      return true;
    }
    usleep(250 * 1000);
  }
  std::cerr << "failed to tune to " << jt->first << " (status " << status << ")" << std::endl;
  return false;
}

bool tuner::track()
{
  if (adapter_ == adapters_.cend()) {
    return false;
  }

  const std::string path = *adapter_ + "/demux0";
  int fd = open(path.c_str(), O_RDWR);
  if (fd == -1) {
    std::perror(("open(" + path + ")").c_str());
    return false;
  }

  struct dmx_pes_filter_params filter;
  filter.pid = 0x2000;
  filter.input = DMX_IN_FRONTEND;
  filter.output = DMX_OUT_TS_TAP;
  filter.pes_type = DMX_PES_VIDEO;
  filter.flags = DMX_IMMEDIATE_START;
  if (ioctl(fd, DMX_SET_PES_FILTER, &filter) == -1) {
    std::perror("ioctl DMX_SET_PES_FILTER");
    close(fd);
    return false;
  }

  demux_fd_ = fd;
  return true;
}

std::unordered_map<int, channel> tuner::parse_channels(std::istream& is)
{
  std::unordered_map<int, channel> channels;
  std::string line;
  while (std::getline(is, line)) {
    std::vector<std::string> fields;
    boost::tokenizer<boost::escaped_list_separator<char> > tok(line);
    std::copy(tok.begin(), tok.end(), std::back_inserter(fields));
    if (fields.size() != 3) {
      std::cerr << "warning: invalid format. ignore this line: " << line << std::endl;
    } else {
      for (std::string& f : fields) {
        boost::trim(f);
      }
      try {
        const int id = boost::lexical_cast<int>(fields[0]);
        const unsigned int freq = boost::lexical_cast<unsigned int>(fields[1]);
        const unsigned int ts_id = boost::lexical_cast<unsigned int>(fields[2]);
        channels.insert(std::make_pair(id, channel(freq, ts_id)));
      } catch (boost::bad_lexical_cast&) {
        std::cerr << "warning: lexical_cast failed. ignore this line: " << line << std::endl;
      }
    }
  }
  return channels;
}

};
