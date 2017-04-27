// -*- c++ -*-
#ifndef REGEXBENCH_REMATCHENGINE_H
#define REGEXBENCH_REMATCHENGINE_H

#include <atomic>
#include <map>

#include <rematch/compile.h>
#include <rematch/execute.h>
#include <rematch/rematch.h>

#include "Engine.h"
#include "Session.h"

namespace regexbench {

class REmatchAutomataEngine : public Engine {
public:
  REmatchAutomataEngine(bool red = false);
  virtual ~REmatchAutomataEngine();

  virtual void compile(const std::vector<Rule>&, size_t = 1);
  virtual void load(const std::string&, size_t = 1);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

private:
  mregflow_t* flow;
  matcher_t* matcher;

protected:
  mregex_t* txtbl;
  mregmatch_t regmatch[1];
  bool reduce = false;
  char __padding[7];
};

class REmatchSOEngine : public Engine {
  using run_func_t = bool (*)(const char*, size_t, matchctx_t*);

public:
  REmatchSOEngine();
  virtual ~REmatchSOEngine();

  virtual void load(const std::string&, size_t = 1);
  virtual size_t match(const char* data, size_t len, size_t, size_t = 0,
                       size_t* = nullptr)
  {
    return run(data, len, ctx);
  }

private:
  run_func_t run;
  matchctx_t* ctx;
  void* dlhandle;
};

#ifdef WITH_SESSION
class REmatchAutomataEngineSession : public REmatchAutomataEngine {
public:
  REmatchAutomataEngineSession();
  virtual ~REmatchAutomataEngineSession();
  virtual void init(size_t);

  using Engine::match;
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

private:
  static constexpr size_t unit_total = 1u << 17;
  mregSession_t* parent;
  mregSession_t* child;
};
#endif

class REmatch2AutomataEngine : public Engine {
public:
  REmatch2AutomataEngine(bool red = false);
  ~REmatch2AutomataEngine();

  void compile(const std::vector<Rule>& rules, size_t = 1) override;
  void compile_test(const std::vector<Rule>&) const override;
  void update_test(const std::vector<Rule>&) override;
  void load(const std::string& file, size_t = 1) override;
  size_t match(const char* pkt, size_t len, size_t, size_t = 0,
               size_t* = nullptr) override;

private:
  void load_updated(const std::string& file);

  // rematch2_t* matcher;
  std::map<int, rematch2_t*> matchers;
  std::vector<rematch_match_context_t*> contexts;
  std::atomic_int version;
  std::vector<int> versions;
  bool reduce = false;

protected:
  char __padding[7];
};

} // namespace regexbench

#endif
