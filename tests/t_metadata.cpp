#include <atf-c++.hpp>

#include "../src/Session.h"
#include "../src/regexbench.h"
asdfasdfasfasdf
using namespace regexbench;

ATF_TEST_CASE_WITHOUT_HEAD(metadata_create);
ATF_TEST_CASE_BODY(metadata_create)
{
  size_t nsessions = 0;
  regexbench::PcapSource pcap(DATA_DIR "/pcap/session.pcap");
  auto match_meta = buildMatchMeta(pcap, nsessions);
  ATF_REQUIRE_EQ(2, nsessions);
  ATF_REQUIRE_EQ(8, match_meta.size());
}

ATF_TEST_CASE_WITHOUT_HEAD(metadata_content);
ATF_TEST_CASE_BODY(metadata_content)
{
  size_t nsessions = 0;
  regexbench::PcapSource pcap(DATA_DIR "/pcap/session2.pcap");
  auto match_meta = buildMatchMeta(pcap, nsessions);
  ATF_REQUIRE_EQ(4, nsessions);
  ATF_REQUIRE_EQ(17, match_meta.size());

  ATF_REQUIRE(MatchMeta(0, 54, 3) == match_meta[2]);
  ATF_REQUIRE(MatchMeta(0, 54, 4) == match_meta[3]);
  ATF_REQUIRE(MatchMeta(2, 54, 3) == match_meta[10]);
  ATF_REQUIRE(MatchMeta(2, 54, 3) == match_meta[11]);
  ATF_REQUIRE(MatchMeta(2, 54, 4) == match_meta[12]);
}

ATF_INIT_TEST_CASES(tcs)
{
  ATF_ADD_TEST_CASE(tcs, metadata_create);
  ATF_ADD_TEST_CASE(tcs, metadata_content);
}
