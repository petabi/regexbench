#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <utility>

#include <hs/hs_compile.h>
#include <hs/hs_runtime.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <rematch/compile.h>
#include <rematch/execute.h>
#include <rematch/rematch.h>

#include "db_setup.h"
#include "../Rule.h"
#include "PcreChecker.h"
#include "CheckerShell.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

using regexbench::Rule;

static void usage()
{
  cerr << "arguments too few" << endl;
  cerr << "command line should look like :" << endl;
  cerr << endl;
  cerr << "for direct result update ..." << endl;
  cerr << "$ pcre_checker {db_file}" << endl;
  cerr << endl;
  cerr << "for shell access ..." << endl;
  cerr << "$ pcre_checker -s" << endl;
}

/*
struct AuxInfo {
  int resMatchId;
  int resNomatchId;
  int resErrorId;
  std::map<string, int> str2EngineId;
  uint32_t nmatch; // this is rematch only parameter

  vector<Rule> rules;
};

static void checkRematch(PcreCheckDb& db, const struct AuxInfo& aux);
static void checkHyperscan(PcreCheckDb& db, struct AuxInfo& aux);
static void checkPcre(PcreCheckDb& db, struct AuxInfo& aux);
*/

static int runShell();

int main(int argc, char **argv)
{
  if (argc < 2) {
    usage();
    return -1;
  }

  string arg(argv[1]);

  if (arg == "-s")
    return runShell(); // Enter shell mode

  // Now direct result update based on the existing database content

  string dbFile(arg);

  try {
    dbFile = "database=" + dbFile;
    PcreCheckDb db("sqlite3", dbFile);
    // db.verbose = true; // turn on when debugging

    if (db.needsUpgrade()) // TODO : revisit!!
      db.upgrade();

    db.begin();

    AuxInfo aux;
    aux.resMatchId =
        select<Result>(db, Result::Name == "match").one().id.value();
    aux.resNomatchId =
        select<Result>(db, Result::Name == "nomatch").one().id.value();
    aux.resErrorId =
        select<Result>(db, Result::Name == "error").one().id.value();
    aux.str2EngineId["rematch"] =
        select<Engine>(db, Engine::Name == "rematch").one().id.value();
    aux.str2EngineId["hyperscan"] =
        select<Engine>(db, Engine::Name == "hyperscan").one().id.value();
    aux.str2EngineId["pcre"] =
        select<Engine>(db, Engine::Name == "pcre").one().id.value();
    aux.nmatch = 10; // TODO
    auto& rules = aux.rules;
    vector<DbRule> dbRules = select<DbRule>(db).orderBy(DbRule::Id).all();
    for (const auto &dbRule : dbRules) {
      auto blob = dbRule.content.value();
      size_t len = blob.length();
      auto temp = std::make_unique<char[]>(len);
      blob.getData(reinterpret_cast<unsigned char*>(temp.get()), len, 0);
      std::string line(temp.get(), len);
      rules.emplace_back(Rule(line, static_cast<size_t>(dbRule.id.value())));
    }
    // for debugging
    //for (const auto &r : rules) {
    //  cout << "rule " << r.getID() << ": " << r.getRegexp() << endl;
    //}

    checkRematch(db, aux);
    checkHyperscan(db, aux);
    checkPcre(db, aux);

    db.commit(); // commit changes (mostly about result)

  } catch (const std::exception &e) {
    cerr << e.what() << endl;
    return -1;
  } catch (Except& e) { // litesql exception
    cerr << e << endl;
    return -1;
  }
  return 0;
}

void checkRematch(PcreCheckDb& db, const struct AuxInfo& aux)
{
  int engineId = aux.str2EngineId.at("rematch");
  int resMatchId = aux.resMatchId;
  int resNomatchId = aux.resNomatchId;
  const auto& rules = aux.rules;

  rematch2_t* matcher;
  rematch_match_context_t* context;
  vector<const char*> rematchExps;
  vector<unsigned> rematchMods;
  vector<unsigned> rematchIds;
  for (const auto& rule : rules) {
    rematchExps.push_back(rule.getRegexp().data());
    rematchIds.push_back(static_cast<unsigned>(rule.getID()));
    uint32_t opt = 0;
    if (rule.isSet(regexbench::MOD_CASELESS))
      opt |= REMATCH_MOD_CASELESS;
    if (rule.isSet(regexbench::MOD_MULTILINE))
      opt |= REMATCH_MOD_MULTILINE;
    if (rule.isSet(regexbench::MOD_DOTALL))
      opt |= REMATCH_MOD_DOTALL;
    rematchMods.push_back(opt);
  }
  matcher = rematch2_compile(rematchIds.data(), rematchExps.data(),
                             rematchMods.data(), rematchIds.size(),
                             true /* reduce */);
  if (!matcher)
    throw std::runtime_error("Could not build REmatch2 matcher.");
  context = rematch2ContextInit(matcher, aux.nmatch);
  if (context == nullptr)
    throw std::runtime_error("Could not initialize context.");

  // prepare data (only need the data specified in Test table)
  int lastPid = -1;
  vector<Test> tests = select<Test>(db).orderBy(Test::Patternid).all();
  for (const auto& t : tests) {
    if (t.patternid.value() == lastPid)
      continue;
    lastPid = t.patternid.value();
    // we can get excption below
    // (which should not happen with a db correctly set up)
    const auto& pattern = select<Pattern>(db, Pattern::Id == lastPid).one();
    auto blob = pattern.content.value();
    size_t len = blob.length();
    auto temp = std::make_unique<char[]>(len);
    blob.getData(reinterpret_cast<unsigned char*>(temp.get()), len, 0);

    // for debugging
    // cout << "pattern " << lastPid << " content : " << string(temp.get(), len)
    //     << endl;

    // set up Rule-id to (Test-id, result) mapping to be used for TestResult
    // update
    std::map<int, std::pair<int, bool>> rule2TestMap;
    auto curTest = select<Test>(db, Test::Patternid == lastPid).cursor();
    for (; curTest.rowsLeft(); curTest++) {
      rule2TestMap[(*curTest).ruleid.value()] =
          std::make_pair((*curTest).id.value(), false);
    }

    // do match
    int ret = rematch2_exec(matcher, temp.get(), len, context);
    if (ret == MREG_FINISHED) { // this means we need to adjust 'nmatch'
                                // parameter used for rematch2ContextInit
      cerr << "rematch2 returned MREG_FINISHED" << endl;
    }
    if (context->num_matches > 0) { // match
      // for debugging
      // cout << "pattern " << lastPid;
      // cout << " matched rules :" << endl;
      for (size_t i = 0; i < context->num_matches; ++i) {
        // for debugging
        // cout << " " << context->matchlist[i].fid;
        stateid_t mid = context->matchlist[i].fid;
        if (rule2TestMap.count(static_cast<int>(mid)) > 0)
          rule2TestMap[static_cast<int>(mid)].second = true;
      }
      // cout << endl;
    } else { // nomatch
      cout << "pattern " << lastPid << " has no match" << endl;
    }
    rematch2ContextClear(context, true);

    // for debugging
    // cout << "Matched rule and test id for pattern id " << lastPid << endl;
    // for (const auto& p : rule2TestMap) {
    //  cout << " rule id " << p.first << " test id " << p.second.first
    //       << " matched? " << p.second.second << endl;
    //}
    for (const auto& p : rule2TestMap) {
      try {
        auto cur =
            select<TestResult>(db, TestResult::Testid == p.second.first &&
                                       TestResult::Engineid == engineId)
                .cursor();
        (*cur).resultid = (p.second.second ? resMatchId : resNomatchId);
        (*cur).update();
      } catch (NotFound) {
        TestResult res(db);
        res.testid = p.second.first;
        res.engineid = engineId;
        res.resultid = (p.second.second ? resMatchId : resNomatchId);
        res.update();
      }
    }
  } // for loop for Test table entries

  // clean-up of REmatch related objects
  rematch2ContextFree(context);
  rematch2Free(matcher);
}

static int hsOnMatch(unsigned int, unsigned long long, unsigned long long,
                   unsigned int, void* ctx)
{
  size_t& nmatches = *static_cast<size_t*>(ctx);
  nmatches++;
  return 0;
}

void checkHyperscan(PcreCheckDb& db, struct AuxInfo& aux)
{
  int engineId = aux.str2EngineId.at("hyperscan");
  int resMatchId = aux.resMatchId;
  int resNomatchId = aux.resNomatchId;
  int resErrorId = aux.resErrorId;
  const auto& rules = aux.rules;

  hs_database_t* hsDb = nullptr;
  hs_scratch_t* hsScratch = nullptr;
  //hs_platform_info_t hsPlatform;
  hs_compile_error_t* hsErr = nullptr;

  auto cur = select<Test>(db).orderBy(Test::Ruleid).cursor();
  if (!cur.rowsLeft()) // nothing to do
    return;

  // map of Test-id to Result-id
  std::map<int, int> test2ResMap;

  // Entering into this loop, please make sure that
  // rules and cur are all sorted w.r.t rule id
  // (only, cur can have multiple occurrences of rule id's)
  // and also rules be super set of the iteration cur points to
  // in terms of containing rule id's.
  // Below outer and inner loop is assuming the above constraints
  // to prevent multiple rule compile for a same rule
  for (const auto& rule : rules) {
    if (!cur.rowsLeft())
      break;
    if (rule.getID() != (*cur).ruleid.value())
      continue;

    unsigned flag = HS_FLAG_ALLOWEMPTY;
    if (rule.isSet(regexbench::MOD_CASELESS))
      flag |= HS_FLAG_CASELESS;
    if (rule.isSet(regexbench::MOD_MULTILINE))
      flag |= HS_FLAG_MULTILINE;
    if (rule.isSet(regexbench::MOD_DOTALL))
      flag |= HS_FLAG_DOTALL;

    hsDb = nullptr;
    hsScratch = nullptr;
    hsErr = nullptr;
    auto resCompile = hs_compile(rule.getRegexp().data(), flag, HS_MODE_BLOCK,
                             nullptr, &hsDb, &hsErr);
    if (resCompile == HS_SUCCESS) {
      auto resAlloc = hs_alloc_scratch(hsDb, &hsScratch);
      if (resAlloc != HS_SUCCESS) {
        hs_free_database(hsDb);
        throw std::bad_alloc();
      }
    } else
      hs_free_compile_error(hsErr);

    for (; cur.rowsLeft() && rule.getID() == (*cur).ruleid.value(); cur++) {

      if (resCompile != HS_SUCCESS) {
        test2ResMap[(*cur).id.value()] = resErrorId;
        continue;
      }

      const auto& pattern =
          select<Pattern>(db, Pattern::Id == (*cur).patternid).one();
      auto blob = pattern.content.value();
      size_t len = blob.length();
      auto temp = std::make_unique<char[]>(len);
      blob.getData(reinterpret_cast<unsigned char*>(temp.get()), len, 0);

      size_t nmatches = 0;
      hs_scan(hsDb, temp.get(), static_cast<unsigned>(len), 0, hsScratch,
              hsOnMatch, &nmatches);
      test2ResMap[(*cur).id.value()] =
          (nmatches > 0) ? resMatchId : resNomatchId;
    }

    hs_free_scratch(hsScratch);
    hs_free_database(hsDb);

  }

  //cout << "Hyper scan result" << endl << endl;
  for (const auto& p : test2ResMap) {
    try {
      auto curT = select<TestResult>(db, TestResult::Testid == p.first &&
                                            TestResult::Engineid == engineId)
                     .cursor();
      (*curT).resultid = p.second;
      (*curT).update();
    } catch (NotFound) {
      TestResult res(db);
      res.testid = p.first;
      res.engineid = engineId;
      res.resultid = p.second;
      res.update();
    }
    // for debugging
    //const auto& test = select<Test>(db, Test::Id == p.first).one();
    //cout << "test " << test.id.value() << " (rule id " << test.ruleid.value()
    //     << ", pattern id " << test.patternid.value()
    //     << ") => result : " << p.second << endl;
  }
}

void checkPcre(PcreCheckDb& db, struct AuxInfo& aux)
{
  int engineId = aux.str2EngineId.at("pcre");
  int resMatchId = aux.resMatchId;
  int resNomatchId = aux.resNomatchId;
  int resErrorId = aux.resErrorId;
  const auto& rules = aux.rules;

  auto cur = select<Test>(db).orderBy(Test::Ruleid).cursor();
  if (!cur.rowsLeft()) // nothing to do
    return;

  // map of Test-id to Result-id
  std::map<int, int> test2ResMap;

  // Entering into this loop, please make sure that
  // rules and cur are all sorted w.r.t rule id
  // (only, cur can have multiple occurrences of rule id's)
  // and also rules be super set of the iteration cur points to
  // in terms of containing rule id's.
  // Below outer and inner loop is assuming the above constraints
  // to prevent multiple rule compile for a same rule
  for (const auto& rule : rules) {
    if (!cur.rowsLeft())
      break;
    if (rule.getID() != (*cur).ruleid.value())
      continue;

    PCRE2_SIZE erroffset = 0;
    int errcode = 0;
    pcre2_code* re =
        pcre2_compile(reinterpret_cast<PCRE2_SPTR>(rule.getRegexp().data()),
                      PCRE2_ZERO_TERMINATED, rule.getPCRE2Options(), &errcode,
                      &erroffset, nullptr);
    pcre2_match_data* mdata = nullptr;
    if (re != nullptr) {
      mdata = pcre2_match_data_create_from_pattern(re, nullptr);
    }

    for (; cur.rowsLeft() && rule.getID() == (*cur).ruleid.value(); cur++) {

      if (re == nullptr) {
        test2ResMap[(*cur).id.value()] = resErrorId;
        continue;
      }

      const auto& pattern =
          select<Pattern>(db, Pattern::Id == (*cur).patternid).one();
      auto blob = pattern.content.value();
      size_t len = blob.length();
      auto temp = std::make_unique<char[]>(len);
      blob.getData(reinterpret_cast<unsigned char*>(temp.get()), len, 0);

      int rc =
          pcre2_match(re, reinterpret_cast<PCRE2_SPTR>(temp.get()), len, 0,
                      PCRE2_NOTEMPTY_ATSTART | PCRE2_NOTEMPTY, mdata, nullptr);

      test2ResMap[(*cur).id.value()] = (rc >= 0) ? resMatchId : resNomatchId;
    }

    pcre2_code_free(re);
    pcre2_match_data_free(mdata);
  }

  //cout << "PCRE match result" << endl << endl;
  for (const auto& p : test2ResMap) {
    try {
      auto curT = select<TestResult>(db, TestResult::Testid == p.first &&
                                            TestResult::Engineid == engineId)
                     .cursor();
      (*curT).resultid = p.second;
      (*curT).update();
    } catch (NotFound) {
      TestResult res(db);
      res.testid = p.first;
      res.engineid = engineId;
      res.resultid = p.second;
      res.update();
    }
    // for debugging
    //const auto& test = select<Test>(db, Test::Id == p.first).one();
    //cout << "test " << test.id.value() << " (rule id " << test.ruleid.value()
    //     << ", pattern id " << test.patternid.value()
    //     << ") => result : " << p.second << endl;
  }
}

int runShell()
{
  try {
    CheckerShell shell;
    shell.initialize();
    shell.run();
  } catch (const std::exception &ex) {
    cerr << ex.what() << endl;
    return -1;
  }

  return 0;
}
