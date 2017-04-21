#ifndef CHECKER_SHELL_H
#define CHECKER_SHELL_H

#include <histedit.h> // libedit
#include <memory>
#include <vector>

#include <pius/auto-opt.h>

#include "db_setup.h"

// clang-format off
// define command structure
AUTO_OPT_ID(attach);
AUTO_OPT_ID(detach);
AUTO_OPT_ID(db);
AUTO_OPT_ID(rule);
AUTO_OPT_ID(pattern);
AUTO_OPT_ID(grammar);
AUTO_OPT_ID(grammars);
AUTO_OPT_ID(engine);
AUTO_OPT_ID(pcre);
AUTO_OPT_ID(rematch);
AUTO_OPT_ID(hyperscan);
AUTO_OPT_ID(test);
AUTO_OPT_ID(cond);
AUTO_OPT_ID(result);
AUTO_OPT_ID(expect);
AUTO_OPT_ID(match);
AUTO_OPT_ID(nomatch);
AUTO_OPT_ID(error);
AUTO_OPT_ID(ID); // id is not allowed :)
AUTO_OPT_ID(name);
AUTO_OPT_ID(content);
AUTO_OPT_ID(desc);
AUTO_OPT_ID(show);
AUTO_OPT_ID(table);
AUTO_OPT_ID(failed);
AUTO_OPT_ID(detailed);
AUTO_OPT_ID(setup);
AUTO_OPT_ID(update);
AUTO_OPT_ID(clear);
AUTO_OPT_ID(from);
AUTO_OPT_ID(json);
AUTO_OPT_ID(singletest);
AUTO_OPT_ID(re);
AUTO_OPT_ID(data);
AUTO_OPT_ID(exit);

using cmd_opts_type = auto_opt_def<
  auto_opt_tbl<idt::attach,
    auto_opt_str<idt::db>
  >,
  auto_opt_tbl<idt::detach,
    auto_opt_null<idt::db>
  >,
  auto_opt_tbl<idt::setup,
    auto_opt_tbl<idt::from,
      auto_opt_str<idt::json>
    >
  >,
  auto_opt_tbl<idt::update,
    auto_opt_null<idt::result>
  >,
  auto_opt_tbl<idt::singletest,
    auto_opt_str<idt::re>,
    auto_opt_str<idt::data>
  >,
  auto_opt_tbl<idt::show,
    auto_opt_tbl<idt::table,
      auto_opt_null<idt::rule>,
      auto_opt_null<idt::pattern>,
      auto_opt_tbl<idt::test,
        auto_opt_null<idt::detailed>,
        auto_opt_tbl<idt::cond,
          auto_opt_str<idt::rule>,
          auto_opt_str<idt::pattern>
        >,
        auto_opt_tbl<idt::result,
          auto_opt_tbl<idt::expect,
            auto_opt_null<idt::match>,
            auto_opt_null<idt::nomatch>,
            auto_opt_null<idt::error>
          >,
          auto_opt_tbl<idt::engine,
            auto_opt_null<idt::pcre>,
            auto_opt_null<idt::rematch>,
            auto_opt_null<idt::hyperscan>
          >,
          auto_opt_null<idt::failed>
        >
      >
    >
  >, // show
  auto_opt_tbl<idt::clear,
    auto_opt_null<idt::result>
  >,
  auto_opt_null<idt::exit>
>;
// clang-format on

class CheckerShell {
public:
  CheckerShell()
      : running(0), el(nullptr), hist(nullptr), tok(nullptr), cmdFunc(*this) {
    if (instance != nullptr)
      throw std::runtime_error("Test shell instance already exists");
    // it's not that unreasonable that
    // we keep a sole instance of shell
    instance = this;
  }
  ~CheckerShell() {
    if (hist != nullptr)
      history_end(hist);
    if (el != nullptr)
      el_end(el);
    if (tok != nullptr)
      tok_end(tok);
  }
  void initialize();
  void run();

private:
  static char *prompt(EditLine *);
  static unsigned char complete(EditLine *e, int ch); // func pointer export
  unsigned char doComplete();                         // does the real job

  static CheckerShell *instance;
  int running;
  // EditLine related pointers
  char _padding[4];
  EditLine *el;
  History *hist;
  Tokenizer *tok;

  // command and option parsing
  cmd_opts_type cmdOpts;
  pius::conf::CmdParseContext cCtx;

public:
  using cmd_attach_option = decltype(cmdOpts[id::attach]);
  using cmd_detach_option = decltype(cmdOpts[id::detach]);
  using cmd_setup_option = decltype(cmdOpts[id::setup]);
  using cmd_update_option = decltype(cmdOpts[id::update]);
  using cmd_show_option = decltype(cmdOpts[id::show]);
  using cmd_clear_option = decltype(cmdOpts[id::clear]);
  using cmd_singletest_option = decltype(cmdOpts[id::singletest]);
  using cmd_exit_option = decltype(cmdOpts[id::exit]);

  using cmd_show_table_test_option =
      decltype(cmdOpts[id::show][id::table][id::test]);

private:
  // dispatcher
  void dispatchCmds(const char *line);
  template <typename T> void processCmd(T &o);
  void processTestTable(cmd_show_table_test_option&);

  struct cmd_functor { // to be used by fusion::map for_each
    cmd_functor(CheckerShell &p) : sh(p) {}
    template <typename T> void operator()(T &t) const {
      if (t.second.isValid()) {
        sh.processCmd(t.second);
      }
    }

  private:
    CheckerShell &sh;
  };
  friend cmd_functor;
  cmd_functor cmdFunc;

  // helper funcs
  template <typename T> bool convertToInt(const std::string &s, T &t);
  bool parseCsvLine(const std::string &s, std::vector<std::string> &v);

  // PCRE related
  std::unique_ptr<PcreCheckDb> pDb;
  std::string dbName;
};
#endif
