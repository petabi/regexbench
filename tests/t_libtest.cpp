#include <atf-c++.hpp>

#include "../src/regexbench.h"
#include <boost/format.hpp>

using namespace regexbench;

static void report(const std::map<std::string, size_t> &m)
{
  std::ostringstream buf;
  std::ofstream outputFile(DATA_DIR "/realtime", std::ios_base::app);

  bool isTotal = m.find("Sec")->second ? false: true;

  if (isTotal)
    buf << "========================================================" << "\n" << "TOTAL ";

  for (const auto& it : m) {
    if (isTotal && it.first == "Sec")
      continue;

    buf << boost::format("%s: %6f ") % it.first % it.second;
  }
  buf << std::endl;

  if (isTotal)
    buf << std::endl;

  outputFile << buf.str();
}

ATF_TEST_CASE_WITHOUT_HEAD(lib_test);
ATF_TEST_CASE_BODY(lib_test)
{
  Arguments args =
      regexbench::init(DATA_DIR "/rule/libtest.re",
                       DATA_DIR "/pcap/libtest.pcap", DATA_DIR "/output.json");
  int result = regexbench::exec(args);

  // hyperscan not support rule
  // id: 5 Possessive quantifiers are not supported. rule:abc.++
  // id: 6 Possessive quantifiers are not supported. rule:abc?+c
  ATF_REQUIRE_EQ(1, result);

  args.engine = EngineType::rematch2;
  result = regexbench::exec(args, report);
  ATF_REQUIRE_EQ(0, result);
}

ATF_INIT_TEST_CASES(tcs) { ATF_ADD_TEST_CASE(tcs, lib_test); }
