// include LiteSQL's header file and generated header file
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

#include "db_setup.h"

// std namespace aliases
using std::cout;
using std::cerr;
using std::endl;
using std::string;

/*
static void usage()
{
  cerr << "arguments too few" << endl;
  cerr << "command line should look like :" << endl;
  cerr << "$ db_setup {json_file} {db_file}" << endl;
}
*/

/*
template <typename T>
void parseNameList(PcreCheckDb& db, const string& member, const Json::Value&);

static void parseRules(PcreCheckDb& db, const Json::Value&);
static void parseGrammars(PcreCheckDb& db, const Json::Value&);
static void parsePatterns(PcreCheckDb& db, const Json::Value&);
static void parseTests(PcreCheckDb& db, const Json::Value&);
*/

/*
int main(int argc, char **argv) {
  if (argc < 3) {
    usage();
    return -1;
  }

  std::ifstream jsonFile(argv[1]);
  string dbFile(argv[2]);

  // Parse json file
  Json::Value root;
  try {
    jsonFile >> root;
  } catch (const std::exception &e) {
    cerr << "json file parse error" << e.what() << endl;
    return -1;
  }

  try {
    dbFile = "database=" + dbFile;
    PcreCheckDb db("sqlite3", dbFile);
    // create tables, sequences and indexes
    //db.verbose = true;

    if (db.needsUpgrade()) {
      db.upgrade();
    }

    // start transaction
    db.begin();


    // Parse 'rules'
    const auto& rules = root["rules"];
    if (!rules.empty())
      parseRules(db, rules);

    // Parse 'grammars'
    const auto& grammars = root["grammars"];
    if (!grammars.empty())
      parseGrammars(db, grammars);

    // Parse 'patterns'
    const auto& patterns = root["patterns"];
    if (!patterns.empty())
      parsePatterns(db, patterns);

    parseNameList<Engine>(db, "engines", root); 
    parseNameList<Result>(db, "results", root); 

    // Parse 'tests' (involves tables 'Test', 'TestGrammar', 'TestResult')
    const auto& tests = root["tests"];
    if (!tests.empty())
      parseTests(db, tests);

    db.commit(); // commit changes
  } catch (const std::exception &e) {
    cerr << "error during parsing" << e.what() << endl;
    return -1;
  } catch (Except e) {
    cerr << e << endl;
    return -1;
  }

  return 0;
}
*/

// currently engines, results
/*
template <typename T>
void parseNameList(PcreCheckDb& db, const string& member,
                   const Json::Value& root)
{
  if (root[member].empty() || !root[member].isArray())
    return;

  for (const auto& e : root[member]) {
    if (!e.isString())
      continue;
    try {
      select<T>(db, T::Name == e.asString()).one();
    } catch (NotFound) {
      T t(db);
      t.name = e.asString();
      t.update();
    }
  }
}
*/

void parseRules(PcreCheckDb& db, const Json::Value& rules)
{
  if (!rules.isArray()) {
    throw std::runtime_error("rules should be array type");
  }

  // name : string (32)
  // content : blob
  // desc : string (2048)
  for (const auto& rule: rules) {
    if (rule["name"].empty() || !rule["name"].isString())
      throw std::runtime_error("rule name must be specfied (as string)");
    const auto &name = rule["name"].asString();
    if (rule["content"].empty() || !rule["content"].isString())
      throw std::runtime_error("rule content must be specfied (as string)");
    const auto &content = rule["content"].asString();

    DbRule rule_db(db);
    try {
      select<DbRule>(db, DbRule::Name == name).one();
      cerr << "rule entry with name " << name
           << " already exists in DB (skipping this)" << endl;
      continue;
    } catch (NotFound) {
      rule_db.name = name;
    }
    rule_db.content = Blob(content.data(), content.size());
    if (!rule["desc"].empty())
      rule_db.desc = rule["desc"].asString();
    rule_db.update();
  }
}

void parseGrammars(PcreCheckDb& db, const Json::Value& grammars)
{
  if (!grammars.isArray()) {
    throw std::runtime_error("grammars should be array type");
  }

  // name : string (32)
  // desc : string (2048)
  for (const auto& grammar: grammars) {
    if (grammar["name"].empty() || !grammar["name"].isString())
      throw std::runtime_error("grammar name must be specfied (as string)");
    const auto &name = grammar["name"].asString();

    Grammar grammar_db(db);
    try {
      select<Grammar>(db, Grammar::Name == name).one();
      cerr << "grammar entry with name " << name
           << " already exists in DB (skipping this)" << endl;
      continue;
    } catch (NotFound) {
      grammar_db.name = name;
    }
    if (!grammar["desc"].empty())
      grammar_db.desc = grammar["desc"].asString();
    grammar_db.update();
  }
}

void parsePatterns(PcreCheckDb& db, const Json::Value& patterns)
{
  if (!patterns.isArray()) {
    throw std::runtime_error("patterns should be array type");
  }

  // name : string (32)
  // content : blob
  // desc : string (2048)
  for (const auto& pattern: patterns) {
    if (pattern["name"].empty() || !pattern["name"].isString())
      throw std::runtime_error("pattern name must be specfied (as string)");
    const auto &name = pattern["name"].asString();
    if (pattern["content"].empty() || !pattern["content"].isString())
      throw std::runtime_error("pattern content must be specfied (as string)");
    const auto &content = pattern["content"].asString();

    Pattern pattern_db(db);
    try {
      select<Pattern>(db, Pattern::Name == name).one();
      cerr << "pattern entry with name " << name
           << " already exists in DB (skipping this)" << endl;
      continue;
    } catch (NotFound) {
      pattern_db.name = name;
    }
    pattern_db.content = Blob(content.data(), content.size());
    if (!pattern["desc"].empty())
      pattern_db.desc = pattern["desc"].asString();
    pattern_db.update();
  }
}

void parseTests(PcreCheckDb& db, const Json::Value& tests)
{
  if (!tests.isArray()) {
    throw std::runtime_error("tests should be array type");
  }

  std::map<string, int> engineMap;
  std::map<string, int> resultMap;

  std::vector<Engine> engineSels = select<Engine>(db).all();
  for (const auto &e : engineSels) {
    engineMap[e.name.value()] = e.id.value();
  }
  std::vector<Result> resultSels = select<Result>(db).all();
  for (const auto &r : resultSels) {
    resultMap[r.name.value()] = r.id.value();
  }

  // rule => DbRule name
  // pattern => Pattern name
  // grammars => array of Grammar names
  // result => json object of result for each engine
  for (const auto& test: tests) {
    if (test["rule"].empty() || !test["rule"].isString())
      throw std::runtime_error("test rule name must be specfied (as string)");
    if (test["pattern"].empty() || !test["pattern"].isString())
      throw std::runtime_error("test pattern name must be specfied (as string)");
    const auto &rulename = test["rule"].asString();
    const auto &patternname = test["pattern"].asString();

    // find ids of rule, pattern, grammar
    int rule_id;
    int pattern_id;
    try {
      const auto& rule_db = select<DbRule>(db, DbRule::Name == rulename).one();
      rule_id = rule_db.id.value();
      const auto& pattern_db = select<Pattern>(db, Pattern::Name == patternname).one();
      pattern_id = pattern_db.id.value();
    } catch (NotFound e) {
      cerr << "rule(" << rulename << ") or pattern(" << patternname
           << ") not found (" << e << ") (skipping this)" << endl;
      continue;
    }
    // find expect id : this is actually a result id
    int expect_id = 0;
    if (!test["expect"].empty() && test["expect"].isString()) {
      if (resultMap.count(test["expect"].asString()))
        expect_id = resultMap.at(test["expect"].asString());
    }

    // now rule_id, pattern_id, expect_id are valid
    // find out Test table id if any or create one
    int test_id;
    try {
      const auto& test_db = select<Test>(db, Test::Ruleid == rule_id &&
                                                 Test::Patternid == pattern_id)
                                .one();
      test_id = test_db.id.value();
    } catch (NotFound) {
      Test test_db(db);
      test_db.ruleid = rule_id;
      test_db.patternid = pattern_id;
      test_db.expectid = expect_id;
      test_db.update();
      test_id = test_db.id.value();
    }

    std::vector<int> grammar_ids;
    if (!test["grammars"].empty()) {
      const auto& grammars = test["grammars"];
      for (const auto& gr : grammars) {
        if (!gr.isString())
          continue; // TODO
        try {
          const auto& gr_db = select<Grammar>(db, Grammar::Name == gr.asString()).one();
          grammar_ids.push_back(gr_db.id.value());
        } catch (NotFound) {
          // just register this grammar on the fly (w/o description)
          Grammar new_gr(db);
          new_gr.name = gr.asString();
          new_gr.update();
          grammar_ids.push_back(new_gr.id.value());
        }
      }
    }

    for (auto gid : grammar_ids) {
      try {
        select<TestGrammar>(db, TestGrammar::Testid == test_id &&
                                    TestGrammar::Grammarid == gid).one();
      } catch (NotFound) {
        TestGrammar tg(db);
        tg.testid = test_id;
        tg.grammarid = gid;
        tg.update();
      }
    }

    std::map<string, string> verdictMap;
    if (!test["result"].empty() && test["result"].isObject()) {
      const auto& result = test["result"];
      for (const auto& engine : result.getMemberNames()) {
        if (engineMap.find(engine) != engineMap.end()) { // engine names
          const auto& verdict = result[engine].asString();
          if (result[engine].isString() &&
              resultMap.find(verdict) != resultMap.end()) {
            verdictMap[engine] = verdict;
          } else {
            cerr << "result for engine " << engine << " set incorrectly"
                 << endl;
          }
        } else {
          cerr << "unknown engine " << engine << " for result" << endl;
        }
      }
    }

    for (auto e2V : verdictMap) {
      try {
        auto resEntry =
            *(select<TestResult>(db, TestResult::Testid == test_id &&
                                         TestResult::Engineid ==
                                             engineMap[e2V.first])
                  .cursor());

        resEntry.resultid = resultMap[e2V.second];
        resEntry.update();
      } catch (NotFound) {
        TestResult resEntry(db);
        resEntry.testid = test_id;
        resEntry.engineid = engineMap[e2V.first];
        resEntry.resultid = resultMap[e2V.second];
        resEntry.update();
      }
    }
  }

}

