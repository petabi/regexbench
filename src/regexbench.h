// -*- c++ -*-
#ifndef REGEXBENCH_H
#define REGEXBENCH_H

#include "Logger.h"
#include "Rule.h"
#include <boost/timer/timer.hpp>
#include <vector>

namespace regexbench {

class Engine;
class PcapSource;

struct MatchResult {
  MatchResult() : nmatches(0), nmatched_pkts(0) {}

  struct timeval udiff;
  struct timeval sdiff;
  size_t nmatches;
  size_t nmatched_pkts;
};

struct MatchMeta {
  MatchMeta(size_t sid_ = 0, size_t oft_ = 0, size_t len_ = 0)
      : sid(sid_), oft(oft_), len(len_)
  {
  }
  bool operator==(const MatchMeta& rhs)
  {
    return sid == rhs.sid && oft == rhs.oft && len == rhs.len;
  }
  size_t sid;
  size_t oft;
  size_t len;
};

std::vector<MatchMeta> buildMatchMeta(const PcapSource&, size_t&);
uint32_t getPLOffset(const std::string&);
std::vector<Rule> loadRules(const std::string&);
void matchThread(Engine* engine, const PcapSource* src, long repeat,
                 size_t core, size_t sel, const std::vector<MatchMeta>* meta,
                 MatchResult* result, Logger* logger);
std::vector<MatchResult> match(Engine&, const PcapSource&, long,
                               const std::vector<size_t>&,
                               const std::vector<MatchMeta>&,
                               const std::string&);
}
#endif // REGEXBENCH_H
