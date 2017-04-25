#include <dlfcn.h>

#include <stdexcept>

#include <dlfcn.h>

#include <rematch/compile.h>
#include <rematch/execute.h>
#include <rematch/rematch.h>

#include "REmatchEngine.h"

using namespace regexbench;

const char NFA_FUNC_NAME[] = "run";
const char NFA_NSTATES_NAME[] = "nstates";

REmatchAutomataEngine::REmatchAutomataEngine(bool red)
    : flow(nullptr), matcher(nullptr), txtbl(nullptr), reduce(red)
{
}

REmatchAutomataEngine::~REmatchAutomataEngine()
{
  if (flow)
    mregflow_delete(flow);
  if (matcher)
    matcher_delete(matcher);
  if (txtbl)
    mregfree(txtbl);
}

void REmatchAutomataEngine::compile(const std::vector<Rule>& rules, size_t)
{
  std::vector<const char*> exps;
  std::vector<unsigned> mods;
  std::vector<unsigned> ids;
  for (const auto& rule : rules) {
    exps.push_back(rule.getRegexp().data());
    ids.push_back(static_cast<unsigned>(rule.getID()));
    uint32_t opt = 0;
    if (rule.isSet(MOD_CASELESS))
      opt |= REMATCH_MOD_CASELESS;
    if (rule.isSet(MOD_MULTILINE))
      opt |= REMATCH_MOD_MULTILINE;
    if (rule.isSet(MOD_DOTALL))
      opt |= REMATCH_MOD_DOTALL;
    mods.push_back(opt);
  }
  txtbl =
      rematch_compile(ids.data(), exps.data(), mods.data(), ids.size(), reduce);
  flow = mregflow_new(txtbl->nstates, 1, 1);
  matcher = matcher_new(txtbl->nstates);
}

void REmatchAutomataEngine::load(const std::string& filename, size_t)
{
  txtbl = rematchload(filename.c_str());
  if (txtbl == nullptr) {
    throw std::runtime_error("cannot load NFA");
  }
  flow = mregflow_new(txtbl->nstates, 1, 1);
  if (flow == nullptr)
    throw std::bad_alloc();
  matcher = matcher_new(txtbl->nstates);
  if (matcher == nullptr)
    throw std::bad_alloc();
}

size_t REmatchAutomataEngine::match(const char* data, size_t len, size_t,
                                    size_t /*thr*/, size_t* /*pId*/)
{
  mregexec_single(txtbl, data, len, 1, regmatch, matcher, flow);
  return matcher->matches;
}

REmatchSOEngine::REmatchSOEngine()
    : run(nullptr), ctx(nullptr), dlhandle(nullptr)
{
}

REmatchSOEngine::~REmatchSOEngine()
{
  if (ctx)
    destroy_matchctx(ctx);
  if (dlhandle)
    dlclose(dlhandle);
}

void REmatchSOEngine::load(const std::string& filename, size_t)
{
  dlhandle = dlopen(filename.c_str(), RTLD_LAZY);
  if (dlhandle == nullptr) {
    char* error = dlerror();
    if (error != nullptr)
      throw std::runtime_error(error);
    throw std::runtime_error("fail to load " + filename);
  }
  size_t* p = reinterpret_cast<size_t*>(dlsym(dlhandle, NFA_NSTATES_NAME));
  if (p == nullptr) {
    char* error = dlerror();
    if (error != nullptr)
      throw std::runtime_error(error);
    throw std::runtime_error("cannot find symol");
  }
  ctx = create_matchctx(*p);
  if (ctx == nullptr)
    throw std::bad_alloc();
  run = reinterpret_cast<run_func_t>(dlsym(dlhandle, NFA_FUNC_NAME));
  if (run == nullptr) {
    char* error = dlerror();
    if (error != nullptr)
      throw std::runtime_error(error);
    throw std::runtime_error("cannot find symol");
  }
}

#ifdef WITH_SESSION
REmatchAutomataEngineSession::REmatchAutomataEngineSession()
    : parent{nullptr}, child{nullptr}
{
}
REmatchAutomataEngineSession::~REmatchAutomataEngineSession()
{
  mregSession_delete_parent(parent);
  mregSession_delete_child(child);
}

size_t REmatchAutomataEngineSession::match(const char* pkt, size_t len,
                                           size_t idx, size_t /*thr*/,
                                           size_t* /*pId*/)
{
  matcher_t* cur = child->mindex[idx];
  size_t ret = 0;
  switch (mregexec_session(txtbl, pkt, len, 1, regmatch, cur, child)) {
  case MREG_FINISHED: // finished
    cur->num_active = 0;
    ret = cur->matches;
    break;
  case MREG_NOT_FINISHED: // not finished
    ret = 0;
    break;
  case MREG_FAILURE:
  default:
    cur->num_active = 0;
    ret = 0;
  }
  return ret;
}

void REmatchAutomataEngineSession::init(size_t nsessions)
{
  parent = mregSession_create_parent(static_cast<uint32_t>(nsessions),
                                     txtbl->nstates);
  child = mregSession_create_child(parent, unit_total);

  for (size_t i = 0; i < nsessions; i++) {
    matcher_t* cur = child->mindex[i];
    if (cur->num_active) {
      if (child->active1 < MNULL) {
        MATCHER_SESSION_SET_NEW(cur, child);
      } else {
        throw std::bad_alloc();
      }
    }
  }
}
#endif // WITH_SESSION

REmatch2AutomataEngine::REmatch2AutomataEngine(bool red)
    : matcher(nullptr), reduce(red)
{
}
REmatch2AutomataEngine::~REmatch2AutomataEngine()
{
  for (auto context : contexts)
    rematch2ContextFree(context);
  rematch2Free(matcher);
}

void REmatch2AutomataEngine::compile(const std::vector<Rule>& rules,
                                     size_t numThr)
{
  std::vector<const char*> exps;
  std::vector<unsigned> mods;
  std::vector<unsigned> ids;
  for (const auto& rule : rules) {
    exps.push_back(rule.getRegexp().data());
    ids.push_back(static_cast<unsigned>(rule.getID()));
    uint32_t opt = 0;
    if (rule.isSet(MOD_CASELESS))
      opt |= REMATCH_MOD_CASELESS;
    if (rule.isSet(MOD_MULTILINE))
      opt |= REMATCH_MOD_MULTILINE;
    if (rule.isSet(MOD_DOTALL))
      opt |= REMATCH_MOD_DOTALL;
    mods.push_back(opt);
  }
  matcher = rematch2_compile(ids.data(), exps.data(), mods.data(), ids.size(),
                             reduce);
  if (matcher == nullptr) {
    throw std::runtime_error("Could not build REmatch2 matcher.");
  }
  numThreads = numThr;
  contexts.resize(numThreads, nullptr);
}

void REmatch2AutomataEngine::load(const std::string& file, size_t numThr)
{
  matcher = rematch2Load(file.c_str());
  if (matcher == nullptr)
    throw std::runtime_error("Could not load REmatch2 matcher.");
  numThreads = numThr;
  contexts.resize(numThreads, nullptr);
}

size_t REmatch2AutomataEngine::match(const char* pkt, size_t len, size_t,
                                     size_t thr, size_t* pId)
{
  auto context = contexts[thr];
  if (__builtin_expect((context == nullptr), false)) {
    context = contexts[thr] = rematch2ContextInit(matcher, 1);
    if (context == nullptr)
      throw std::runtime_error("Could not initialize context.");
  }
  rematch_scan_block(matcher, pkt, len, context);
  size_t matched = context->num_matches;
  if (context->num_matches > 0)
    *pId = context->matchlist[0].fid;
  rematch2ContextClear(context, true);
  return matched;
}
