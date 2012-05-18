#ifndef TUNER_H
#define TUNER_H
#include <string>
#include <vector>
#include <unordered_map>
#include "utility.h"
namespace recpt1 {

struct channel
{
  unsigned int frequency;
  unsigned int ts_id;
  channel(unsigned int f, unsigned int t)
    : frequency(f), ts_id(t)
  {}
};

class tuner
{
  int frontend_fd_, demux_fd_;
  std::vector<std::string> adapters_;
  DECLTYPE(adapters_)::const_iterator adapter_;
  std::unordered_map<int, channel> channels_;

  bool tune_impl(int ch);

public:
  tuner(const std::vector<std::string>& adapters, const std::unordered_map<int, channel>& channels);
  ~tuner();
  bool tune(int ch);
  bool track();
  inline std::string adapter() const { return *adapter_; }

  static decltype(channels_) parse_channels(std::istream& is);
};

};
#endif
