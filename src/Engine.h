// -*- c++ -*-
#ifndef REGEXBENCH_ENGINE_H
#define REGEXBENCH_ENGINE_H

#include <vector>

#include "Rule.h"

namespace regexbench {

class Engine {
public:
  virtual ~Engine();

  virtual void compile(const std::vector<Rule>&, size_t = 1) {}
  virtual void compile_test(const std::vector<Rule>&) const {}
  virtual void update_test(const std::vector<Rule>&) {}
  virtual void init(size_t) {}
  virtual size_t getNumThreads() { return numThreads; }
  virtual void load(const std::string&, size_t = 1) {}
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr) = 0;

protected:
  size_t numThreads = 1;
};

} // namespace regexbench

#endif
