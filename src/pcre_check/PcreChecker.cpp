#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <utility>

#include <rematch/compile.h>
#include <rematch/execute.h>
#include <rematch/rematch.h>

#include "db_setup.h"
#include "../Rule.h"

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
  cerr << "$ pcre_checker {db_file}" << endl;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    usage();
    return -1;
  }

  string dbFile(argv[1]);
  cout << "db file " << dbFile << endl;

  try {
    dbFile = "database=" + dbFile;
    PcreCheckDb db("sqlite3", dbFile);
    // db.verbose = true; // turn on when debugging

    if (db.needsUpgrade()) // TODO : revisit!!
      db.upgrade();

    db.begin();

    vector<Rule> rules; // to be used for engine eval
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
    for (const auto &r : rules) {
      cout << "rule " << r.getID() << ": " << r.getRegexp() << endl;
    }

    int engineId;
    int resMatchId =
        select<Result>(db, Result::Name == "match").one().id.value();
    int resNomatchId =
        select<Result>(db, Result::Name == "nomatch").one().id.value();

    int resErrorId =
        select<Result>(db, Result::Name == "error").one().id.value();

    // rematch test first (TODO)
    engineId = select<Engine>(db, Engine::Name == "rematch").one().id.value();
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
    context = rematch2ContextInit(matcher, 10); // nmatch = 10 (TODO)
    if (context == nullptr)
      throw std::runtime_error("Could not initialize context.");

    // prepare data (only need the data specified in Test table)
    int lastPid = -1;
    vector<Test> tests = select<Test>(db).orderBy(Test::Patternid).all();
    for (const auto &t : tests) {
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
      //cout << "pattern " << lastPid << " content : " << string(temp.get(), len)
      //     << endl;

      // set up Rule-id to (Test-id, result) mapping to be used for TestResult update
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
        //cout << "pattern " << lastPid;
        //cout << " matched rules :" << endl;
        for (size_t i = 0; i < context->num_matches; ++i) {
          // for debugging
          //cout << " " << context->matchlist[i].fid;
          stateid_t mid = context->matchlist[i].fid;
          if (rule2TestMap.count(static_cast<int>(mid)) > 0)
            rule2TestMap[mid].second = true;
        }
        //cout << endl;
      } else { // nomatch
        cout << "pattern " << lastPid << " has no match" << endl;
      }
      rematch2ContextClear(context, true);

      // for debugging
      //cout << "Matched rule and test id for pattern id " << lastPid << endl;
      //for (const auto& p : rule2TestMap) {
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



