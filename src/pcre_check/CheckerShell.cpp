#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>

#include <fcntl.h>

#include <jsoncpp/json/json.h>

#include "PcreChecker.h"
#include "CheckerShell.h"

using std::cout;
using std::cerr;
using std::endl;

using std::vector;

using CS = CheckerShell;

CS *CS::instance = nullptr;

void CS::initialize() {
  el = el_init("CheckerShell", stdin, stdout, stderr);
  el_set(el, EL_PROMPT, &CS::prompt);
  el_set(el, EL_EDITOR, "emacs");

  // initialize the history
  hist = history_init();
  if (hist == nullptr) {
    throw std::runtime_error("Checker shell history init error");
  }

  // set the size of the history
  HistEvent histEv;
  history(hist, &histEv, H_SETSIZE, 100);
  // sets up the callback for history
  el_set(el, EL_HIST, history, hist);
  // for tab command completion
  el_set(el, EL_ADDFN, "complete", "command completion", &CS::complete);
  el_set(el, EL_BIND, "\t", "complete", nullptr);

  tok = tok_init(nullptr);
  cmdOpts[id::show][id::table].setExcl(); // one of rule, pattern, test
  cmdOpts[id::show][id::table][id::test][id::result][id::expect].setExcl();
  cmdOpts[id::show][id::table][id::test][id::result][id::engine].setExcl();
}

void CS::run() {
  int count;
  const char *line;
  HistEvent histEv;
  running = 1;
  while (running) {
    line = el_gets(el, &count);

    if (count > 0 && line[0] != '\n') {
      history(hist, &histEv, H_ENTER, line);
      dispatchCmds(line);
    }
    tok_reset(tok);
  }
}

char *CS::prompt(EditLine *) {
  static char prStr[16] = "PCREChk:$ ";
  return prStr; // shell prompt
}

unsigned char CS::complete(EditLine *, int /*ch*/) {
  if (instance)
    return instance->doComplete();
  return CC_ERROR;
}

//
// extracts common string part from the list of strings
// and returns the length of the common string
//
static size_t getCommon(const std::vector<std::string> &cands,
                        std::string &common, size_t offset = 0) {
  // caller should guarantee that [0, offset) range of every cands strings are
  // equal because this function assumes so
  size_t newLen = offset; // value to return
  auto szCands = cands.size();
  size_t ovrlap = offset;

  if (szCands < 1) { // just to be sure
    common = "";
    return 0;
  }
  if (szCands > 1) {
    ovrlap = cands[0].size();
  }
  for (size_t i = 0; i < szCands && (i + 1 < szCands); ++i) {
    ovrlap = std::min(std::min(cands[i].size(), cands[i + 1].size()), ovrlap);
    for (; ovrlap > offset; --ovrlap) {
      auto r = cands[i].compare(offset, ovrlap - offset, cands[i + 1], offset,
                                ovrlap - offset);
      if (r == 0)
        break;
    }
    if (ovrlap <= offset) {
      newLen = ovrlap;
      break;
    }
    newLen = ovrlap;
  }

  common = cands[0].substr(0, newLen);
  return newLen;
}

//#define LINE_MAX 256
unsigned char CS::doComplete() {
  static char line[LINE_MAX + 1]; // including null character
  static size_t lastCurpos = 0;
  static size_t lastLen = 0;
  static bool first = true;
  unsigned char ret = CC_REDISPLAY;
  const LineInfo *lf;
  bool list = false;
  int cmpl = 1; // complete last argument
  lf = el_line(el);

  size_t len = static_cast<size_t>(lf->lastchar - lf->buffer);
  size_t curpos = static_cast<size_t>(lf->cursor - lf->buffer);

  if (len > LINE_MAX)
    return CC_ERROR;

  if (curpos != len) // only supports when cursor is on last character
    return CC_ERROR;

  if ((!first && len == 0) ||
      (len > 0 && len == lastLen && lf->cursor == lf->lastchar &&
       curpos == lastCurpos && memcmp(line, lf->buffer, len) == 0))
    list = true;

  if (first)
    first = false;

  if (!list) {
    memcpy(line, lf->buffer, len);
    line[len] = '\0';
    lastLen = len;
    lastCurpos = static_cast<size_t>(lf->cursor - lf->buffer);
  }

  tok_reset(tok);
  cmdOpts.reset();
  int cmdsC;
  const char **cmds;

  if (len > 0) {
    int r = tok_str(tok, line, &cmdsC, &cmds);
    if (r != 0)
      return CC_ERROR;
    if (line[len - 1] == ' ') // new argument completion
      cmpl = 2;
    cCtx.reset(cmdsC, cmds, cmpl, list); // completion mode
  } else {
    cCtx.reset(0, nullptr, 2, list); // cmpletion mode 2 (new arg)
  }

  try {
    cmdOpts.parseCmdline(cCtx);
    auto szCmpl = cCtx.getSzCmpl();
    auto lenCmd = cCtx.getLenCmd(); // last command size
    std::string insertStr;
    if (szCmpl == 1) {
      insertStr = cCtx.getCmpl()[0];
      insertStr += " ";
      el_insertstr(el, insertStr.c_str() + lenCmd);
      ret = CC_REFRESH;
    } else if (szCmpl > 1) {
      auto lenCommon = getCommon(cCtx.getCmpl(), insertStr, lenCmd);
      if (lenCommon > lenCmd) {
        el_insertstr(el, insertStr.c_str() + lenCmd);
        ret = CC_REFRESH;
      }
      if (list && ret != CC_REFRESH) {
        std::cout << "\n";
        std::cout << "command candidates ...\n";
        for (auto &c : cCtx.getCmpl())
          std::cout << " " << c << "\n";
      }
    }
  } catch (std::runtime_error & /*ex*/) {
    ret = CC_ERROR;
  }

  return ret;
}

static std::unique_ptr<char[]> blob2String(litesql::Blob blob)
{
  size_t len = blob.length();
  auto ret = std::make_unique<char[]>(len + 1);
  blob.getData(reinterpret_cast<unsigned char*>(ret.get()), len, 0);
  ret[len] = '\0';
  return ret;
}

template <> void CS::processCmd(CS::cmd_attach_option &opt) {
  //std::cout << "\"attach\" command\n";
  if (pDb) {
    // Note that unique_ptr can be converted to bool
    // Converted value is true if it owns an object
    cerr << "Already attached to a DB " << dbName << endl;
    cerr << "Detach first" << endl;
    return;
  }

  if (!opt[id::db].isValid()) {
    cerr << "command incomplete : must specify db file" << endl;
    return;
  }

  dbName = opt[id::db](); // should be string
  try {
    pDb.reset(new PcreCheckDb("sqlite3", "database=" + dbName));
    if (pDb->needsUpgrade()) // TODO : revisit!!
      pDb->upgrade();

    pDb->begin();
    cout << "DB attached" << endl;
  } catch (const std::exception &e) {
    cerr << "error while attaching to db file " << dbName << endl;
    cerr << e.what() << endl;
    if (pDb)
      pDb.reset();
  } catch (Except& e) {
    cerr << "error while attaching to db file " << dbName << endl;
    cerr << e << endl;
    if (pDb)
      pDb.reset();
  }
}

template <> void CS::processCmd(CS::cmd_detach_option &opt) {
  //cout << "\"detach\" command" << endl;
  if (!pDb) {
    cerr << "Nothing to detach" << endl;
    return;
  }
  pDb.reset();
  dbName = "";
  cout << "DB detached" << endl;
}

template <> void CS::processCmd(CS::cmd_setup_option &opt) {
  if (opt[id::from].isValid() && opt[id::from][id::json].isValid()) {
    std::ifstream jsonFile(opt[id::from][id::json]());

    // Parse json file
    Json::Value root;
    try {
      jsonFile >> root;
    } catch (const std::exception& e) {
      cerr << "json file parse error" << e.what() << endl;
      return;
    }

    // now play with DB
    try {

      // Parse 'rules'
      const auto& rules = root["rules"];
      if (!rules.empty())
        parseRules(*pDb, rules);

      // Parse 'grammars'
      const auto& grammars = root["grammars"];
      if (!grammars.empty())
        parseGrammars(*pDb, grammars);

      // Parse 'patterns'
      const auto& patterns = root["patterns"];
      if (!patterns.empty())
        parsePatterns(*pDb, patterns);

      parseNameList<Engine>(*pDb, "engines", root);
      parseNameList<Result>(*pDb, "results", root);

      // Parse 'tests' (involves tables 'Test', 'TestGrammar', 'TestResult')
      const auto& tests = root["tests"];
      if (!tests.empty())
        parseTests(*pDb, tests);

      pDb->commit(); // commit changes

    } catch (const std::exception& e) {
      cerr << "error during parsing" << e.what() << endl;
      return;
    } catch (Except e) {
      cerr << e << endl;
      return;
    }
  } // setup from json command
}

template <> void CS::processCmd(CS::cmd_update_option &opt)
{
  try {

    AuxInfo aux;
    aux.resMatchId =
        select<Result>(*pDb, Result::Name == "match").one().id.value();
    aux.resNomatchId =
        select<Result>(*pDb, Result::Name == "nomatch").one().id.value();
    aux.resErrorId =
        select<Result>(*pDb, Result::Name == "error").one().id.value();
    aux.str2EngineId["rematch"] =
        select<Engine>(*pDb, Engine::Name == "rematch").one().id.value();
    aux.str2EngineId["hyperscan"] =
        select<Engine>(*pDb, Engine::Name == "hyperscan").one().id.value();
    aux.str2EngineId["pcre"] =
        select<Engine>(*pDb, Engine::Name == "pcre").one().id.value();
    aux.nmatch = 10; // TODO
    auto& rules = aux.rules;
    vector<DbRule> dbRules = select<DbRule>(*pDb).orderBy(DbRule::Id).all();
    for (const auto &dbRule : dbRules) {
      auto blob = dbRule.content.value();
      size_t len = blob.length();
      auto temp = std::make_unique<char[]>(len);
      blob.getData(reinterpret_cast<unsigned char*>(temp.get()), len, 0);
      std::string line(temp.get(), len);
      rules.emplace_back(regexbench::Rule(line, static_cast<size_t>(dbRule.id.value())));
    }
    // for debugging
    //for (const auto &r : rules) {
    //  cout << "rule " << r.getID() << ": " << r.getRegexp() << endl;
    //}

    checkRematch(*pDb, aux);
    checkHyperscan(*pDb, aux);
    checkPcre(*pDb, aux);

    pDb->commit(); // commit changes (mostly about result)

  } catch (const std::exception &e) {
    cerr << e.what() << endl;
    return;
  } catch (Except& e) { // litesql exception
    cerr << e << endl;
    return;
  }



}

// TODO : move this to header file
template <typename Table>
void setIdFromName(PcreCheckDb& db, const std::string& name, int& id)
{
  try {
    const auto& entry = select<Table>(db, Table::Name == name).one();
    id = entry.id.value();
  } catch (NotFound) {
  }
}

template <typename Table>
void setNameFromId(PcreCheckDb& db, const int id, std::string& name)
{
  try {
    const auto& entry = select<Table>(db, Table::Id == id).one();
    name = entry.name.value();
  } catch (NotFound) {
  }
}

void CS::processTestTable(CS::cmd_show_table_test_option& opt)
{
  // TODO : this implementation is brute force
  // use join and select query if possible (using litesql features)
  int condRule = 0;
  int condPattern = 0;
  //vector<int> condGrammars;
  int condExpect = 0; // expect id
  bool condFailed = false;
  int condEngine = 0;
  bool showContent = opt[id::detailed].isValid() ? true : false;

  if (opt[id::cond].isValid()) {
    auto &cond = opt[id::cond];
    if (cond[id::rule].isValid())
      setIdFromName<DbRule>(*pDb, cond[id::rule](), condRule);
    if (cond[id::pattern].isValid())
      setIdFromName<Pattern>(*pDb, cond[id::pattern](), condRule);
  }
  if (opt[id::result].isValid()) {
    auto &result = opt[id::result];
    if (result[id::expect].isValid()) {
      if (result[id::expect][id::match].isValid())
        setIdFromName<Result>(*pDb, "match", condExpect);
      if (result[id::expect][id::nomatch].isValid())
        setIdFromName<Result>(*pDb, "nomatch", condExpect);
      if (result[id::expect][id::error].isValid())
        setIdFromName<Result>(*pDb, "error", condExpect);
    }
    if (result[id::engine].isValid()) {
      if (result[id::engine][id::pcre].isValid())
        setIdFromName<Engine>(*pDb, "pcre", condEngine);
      if (result[id::engine][id::rematch].isValid())
        setIdFromName<Engine>(*pDb, "rematch", condEngine);
      if (result[id::engine][id::hyperscan].isValid())
        setIdFromName<Engine>(*pDb, "hyperscan", condEngine);
    }
    if (result[id::failed].isValid())
      condFailed = true;
  }

  vector<Test> tests = select<Test>(*pDb).all();
  for (auto& t : tests) {
    if (condRule > 0 && t.ruleid.value() != condRule)
      continue;
    if (condPattern > 0 && t.patternid.value() != condPattern)
      continue;
    if (condExpect > 0 && t.expectid.value() != condExpect)
      continue;

    int testId = t.id.value();

    vector<TestResult> results;
    if (condExpect > 0) {
      results =
          condFailed
              ? select<TestResult>(*pDb, TestResult::Testid == testId &&
                                             TestResult::Resultid != condExpect)
                    .all()
              : select<TestResult>(*pDb, TestResult::Testid == testId &&
                                             TestResult::Resultid == condExpect)
                    .all();
      if (results.empty())
        continue;
    } else {
      results =
          condFailed
              ? select<TestResult>(*pDb, TestResult::Testid == testId &&
                                             TestResult::Resultid !=
                                                 t.expectid.value())
                    .all()
              : select<TestResult>(*pDb, TestResult::Testid == testId).all();
      if (condFailed && results.empty())
        continue;
    }

    std::string ruleName;
    std::string patternName;
    std::string expectName;
    setNameFromId<DbRule>(*pDb, t.ruleid.value(), ruleName);
    setNameFromId<Pattern>(*pDb, t.patternid.value(), patternName);
    setNameFromId<Result>(*pDb, t.expectid.value(), expectName);

    cout << "ID : " << t.id.value() << " (" << ruleName << ", " << patternName << ")" <<
      " expect : " << expectName << " results... ";
    if (results.empty())
      cout << " (empty)";
    for (const auto& r : results) {
      if (condEngine > 0 && r.engineid.value() != condEngine)
        continue;
      std::string engineName;
      std::string resultName;
      setNameFromId<Engine>(*pDb, r.engineid.value(), engineName);
      setNameFromId<Result>(*pDb, r.resultid.value(), resultName);
      cout << engineName << "=>" << resultName << " ";
    }

    cout << endl;
    if (showContent) {
      const auto &r = select<DbRule>(*pDb, DbRule::Id == t.ruleid.value()).one();
      const auto &p = select<Pattern>(*pDb, Pattern::Id == t.patternid.value()).one();
      auto rContent = blob2String(r.content.value()); // char[]
      auto pContent = blob2String(p.content.value()); // char[]
      cout << "  rule : " << rContent.get() << endl;
      cout << "  pattern : " << pContent.get() << endl;
    }
  }
}

template <> void CS::processCmd(CS::cmd_show_option &opt) {
  // show table
  if (!opt[id::table].isValid()) {
    cerr << "command incomplete" << endl;
    return;
  }
  if (!pDb) {
    cerr << "DB must be attached beforehand" << endl;
    return;
  }
  auto &tbl = opt[id::table];
  try {
    if (tbl[id::rule].isValid()) {
      vector<DbRule> dbRules = select<DbRule>(*pDb).all();
      for (const auto& r : dbRules) {
        auto content = blob2String(r.content.value()); // char[]
        cout << "ID : " << r.id.value() << " (" << r.name.value()
             << ") =>" << content.get() << endl;
      }
    }
    if (tbl[id::pattern].isValid()) {
      vector<Pattern> patterns = select<Pattern>(*pDb).all();
      for (const auto& p : patterns) {
        auto content = blob2String(p.content.value()); // char[]
        cout << "ID : " << p.id.value() << " (" << p.name.value()
             << ") =>" << content.get() << endl;
      }
    }
    if (tbl[id::test].isValid()) {
      processTestTable(tbl[id::test]);
    }
  } catch (const Except& e) { // litesql exception
    cerr << e << endl;
  }
}

template <> void CS::processCmd(CS::cmd_clear_option &opt)
{
  if (!pDb) {
    cerr << "DB must be attached beforehand" << endl;
    return;
  }

  if (opt[id::result].isValid()) {
    // clear TestResult table contents
    pDb->query("DELETE FROM " + TestResult::table__);
  }
  pDb->commit();
  cout << "table " << TestResult::table__ << " cleared" << endl;
}

template <> void CS::processCmd(CS::cmd_exit_option &) {
  // exit from the shell
  std::cout << "bye\n";
  running = 0;
}

void CS::dispatchCmds(const char *line) {
  int cmdsC, ret;
  const char **cmds;
  tok_reset(tok); // this is needed because of tab completion
  ret = tok_line(tok, el_line(el), &cmdsC, &cmds, nullptr, nullptr);
  if (ret != 0 || line == nullptr) {
    if (line != nullptr)
      std::cerr << "line tokenizing fail\n";
    return;
  }
  if (!cmdsC) {
    return;
  }

  cmdOpts.reset();
  cCtx.reset(cmdsC, cmds);

  try {
    cmdOpts.parseCmdline(cCtx);
  } catch (std::runtime_error &ex) {
    std::cerr << "error : command parsing fail\n";
    std::cerr << ex.what() << "\n";
    return;
  }

  if (cmdOpts.count(true) == 1) {
    // command match
    cmdOpts.for_each(cmdFunc);
  } else {
    if (cmdOpts.count(true) > 1)
      std::cerr << "error : multiple commands provided\n";
    else
      std::cerr << "error : no matching command found\n";
  }
}

template <typename T> bool CS::convertToInt(const std::string &s, T &t) {
  static_assert(std::is_integral<T>::value, "integer required");
  char *end;
  long ret = strtol(s.c_str(), &end, 10);
  if (end != nullptr && *end == '\0') { // all characters are valid integral
    t = static_cast<T>(ret);
    return true;
  }
  return false;
}

bool CS::parseCsvLine(const std::string &s, std::vector<std::string> &v) {
  std::string ifcStr = s;
  std::replace(ifcStr.begin(), ifcStr.end(), ',', ' ');
  std::istringstream is(ifcStr);
  v = std::vector<std::string>((std::istream_iterator<std::string>(is)),
                               std::istream_iterator<std::string>());
  if (!v.empty())
    return true;
  return false;
}
