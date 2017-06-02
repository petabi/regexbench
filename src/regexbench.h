// -*- c++ -*-
#ifndef REGEXBENCH_H
#define REGEXBENCH_H

#include "Logger.h"
#include "Rule.h"
#include <boost/timer/timer.hpp>
#include <vector>

enum class EngineType : uint64_t {
  boost,
  std_regex,
  hyperscan,
  pcre2,
  pcre2_jit,
  re2,
  rematch,
  rematch2
};

struct Arguments {
  std::string output_file;
  std::string log_file;
  std::string pcap_file;
  std::string rule_file;
  std::string update_pipe;
  std::string compile_time;
  EngineType engine;
  int32_t repeat;
  uint32_t pcre2_concat;
  uint32_t rematch_session;
  uint32_t num_threads;
  uint32_t compile_test;
  uint32_t nmatch = 0;
  std::vector<size_t> cores;
  bool reduce = {false};
  bool quiet = {false};
#ifdef USE_TURBO
  bool turbo = {false};
  char paddings[5];
#else
  char paddings[6];
#endif
};

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

int setAffinity(size_t core, const std::string& thrName = "");
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
std::string compileReport(struct rusage& compileBegin,
                          struct rusage& compileEnd, PcapSource& pcap, bool quiet);
void report(std::string& prefix, PcapSource& pcap, Arguments& args,
            std::vector<MatchResult>& results);
Arguments init(const std::string& rule_file, const std::string& pcap_file,
               const std::string& output_file,
               const EngineType& engine = EngineType::hyperscan,
               uint32_t nthreads = 1, const std::string& affinity = "0",
               int32_t repeat = 1);
int exec(Arguments& args);
}

std::vector<size_t> setup_affinity(size_t num, const std::string& arg);
#endif // REGEXBENCH_H
