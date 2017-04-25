#include "config.h"

#include <atf-c++.hpp>

#include <iostream>

#include "../src/BoostEngine.h"
#include "../src/CPPEngine.h"
#ifdef HAVE_HYPERSCAN
#include "../src/HyperscanEngine.h"
#endif
#include "../src/PcapSource.h"
#ifdef HAVE_PCRE2
#include "../src/PCRE2Engine.h"
#endif
#ifdef HAVE_RE2
#include "../src/RE2Engine.h"
#endif
#include "../src/regexbench.h"
#ifdef HAVE_REMATCH
#include "../src/REmatchEngine.h"
#endif

using namespace regexbench;

std::vector<regexbench::MatchResult> opTest(Engine*);

std::vector<regexbench::MatchResult> opTest(Engine* engine)
{
  engine->compile(regexbench::loadRules(DATA_DIR "/rule/option.re"));
  size_t nsessions = 0;
  regexbench::PcapSource pcap(DATA_DIR "/pcap/option.pcap");
  auto match_info = buildMatchMeta(pcap, nsessions);
  engine->init(nsessions);
  return regexbench::match(*engine, pcap, 1, std::vector<size_t>(), match_info,
                           "");
}

ATF_TEST_CASE_WITHOUT_HEAD(t_option);
ATF_TEST_CASE_BODY(t_option)
{
  std::vector<regexbench::MatchResult> results;

  BoostEngine bengine;
  results = opTest(&bengine);
  ATF_REQUIRE_EQ(3, results[0].nmatches);

  CPPEngine cengine;
  results = opTest(&cengine);
  ATF_REQUIRE_EQ(2, results[0].nmatches);

#ifdef HAVE_HYPERSCAN
  HyperscanEngine hsengine;
  results = opTest(&hsengine);
  ATF_REQUIRE_EQ(3, results[0].nmatches);
#endif

#ifdef HAVE_PCRE2
  PCRE2Engine pengine;
  results = opTest(&pengine);
  ATF_REQUIRE_EQ(3, results[0].nmatches);
#endif

#ifdef HAVE_RE2
  RE2Engine re2engine;
  results = opTest(&re2engine);
  ATF_REQUIRE_EQ(3, results[0].nmatches);
#endif

#ifdef HAVE_REMATCH
  REmatchAutomataEngine rengine;
  results = opTest(&rengine);
  ATF_REQUIRE_EQ(3, results[0].nmatches);
#endif
}

ATF_INIT_TEST_CASES(tcs) { ATF_ADD_TEST_CASE(tcs, t_option); }
