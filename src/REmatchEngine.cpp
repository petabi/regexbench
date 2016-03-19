#include <iostream>
#include <rematch/compile.h>
#include <rematch/execute.h>
#include <rematch/rematch.h>

#include "REmatchEngine.h"

using namespace regexbench;

REmatchEngine::REmatchEngine()
    : flow(nullptr), matcher(nullptr), txtbl(nullptr) {
}

REmatchEngine::~REmatchEngine() {
  if (flow)
    mregflow_delete(flow);
  if (matcher)
    matcher_delete(matcher);
  if (txtbl)
    mregfree(txtbl);
}

void REmatchEngine::compile(const std::vector<Rule> &rules) {
  std::vector<const char *> exps;
  std::vector<unsigned> mods;
  std::vector<unsigned> ids;
  for (const auto &rule : rules) {
    exps.push_back(rule.getRegexp().data());
    ids.push_back(static_cast<unsigned>(rule.getID()));
    mods.push_back(rule.getPCRE2Options());
  }
  txtbl = rematch_compile(ids.data(), exps.data(), mods.data(), ids.size());
  flow = mregflow_new(txtbl->nstates, 1, 1);
  matcher = matcher_new(txtbl->nstates);
}

bool REmatchEngine::match(const char *data, size_t len) {
  MATCHER_SINGLE_CLEAN(matcher);
  mregexec_single(txtbl, data, len, 1, regmatch, matcher, flow);
  return matcher->matches > 0;
}

bool REmatchEngine::load(const std::string &NFAFile) {
  txtbl = rematchload(NFAFile.c_str());
  if (txtbl == nullptr) {
    std::cerr << "cannot load NFA: " << NFAFile << std::endl;
    return false;
  }
  flow = mregflow_new(txtbl->nstates, 1, 1);
  matcher = matcher_new(txtbl->nstates);
  return true;
}
