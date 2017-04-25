// -*- c++ -*-
#ifndef REGEXBENCH_CPPENGINE_H
#define REGEXBENCH_CPPENGINE_H

#include <regex>
#include <vector>

#include "Engine.h"

namespace regexbench {

class CPPEngine : public Engine {
public:
  CPPEngine() = default;
  virtual ~CPPEngine() = default;

  virtual void compile(const std::vector<Rule>&, size_t);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

private:
  std::vector<std::unique_ptr<std::regex>> res;
};

} // namespace regexbench

#endif
