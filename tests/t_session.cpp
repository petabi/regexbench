#include "config.h"

#include <atf-c++.hpp>

#include <iostream>

#include "../src/Session.h"
#include "../src/regexbench.h"
#ifdef HAVE_REMATCH
#include "../src/REmatchEngine.h"
#endif

using namespace regexbench;

ATF_TEST_CASE_WITHOUT_HEAD(session1);
ATF_TEST_CASE_BODY(session1)
{
#ifdef HAVE_REMATCH
  REmatchAutomataEngineSession engine;
  engine.compile(regexbench::loadRules(DATA_DIR "/rule/session.re"));
  size_t nsessions = 0;
  regexbench::PcapSource pcap(DATA_DIR "/pcap/session.pcap");
  auto match_info = buildMatchMeta(pcap, nsessions);
  engine.init(nsessions);
  std::vector<regexbench::MatchResult> results =
      match(engine, pcap, 1, std::vector<size_t>(), match_info, "");
  ATF_REQUIRE_EQ(1, results[0].nmatches);
#endif
}

ATF_TEST_CASE_WITHOUT_HEAD(session2);
ATF_TEST_CASE_BODY(session2)
{
#ifdef HAVE_REMATCH
  REmatchAutomataEngineSession engine;
  engine.compile(regexbench::loadRules(DATA_DIR "/rule/session2.re"));
  size_t nsessions = 0;
  regexbench::PcapSource pcap(DATA_DIR "/pcap/session2.pcap");
  auto match_info = buildMatchMeta(pcap, nsessions);
  ATF_REQUIRE_EQ(4, nsessions);
  engine.init(nsessions);
  std::vector<regexbench::MatchResult> results =
      match(engine, pcap, 1, std::vector<size_t>(), match_info, "");
  ATF_REQUIRE_EQ(2, results[0].nmatches);
#endif
}

ATF_INIT_TEST_CASES(tcs)
{
  ATF_ADD_TEST_CASE(tcs, session1);
  ATF_ADD_TEST_CASE(tcs, session2);
}
