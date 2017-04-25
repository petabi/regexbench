// -*- c++ -*-
#ifndef REGEXBENCH_RE2ENGINE_H
#define REGEXBENCH_RE2ENGINE_H

#include <re2/re2.h>
#include <vector>

#include "Engine.h"

namespace regexbench {

class RE2Engine : public Engine {
public:
  RE2Engine() = default;
  virtual ~RE2Engine() = default;

  virtual void compile(const std::vector<Rule>&, size_t = 1);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

private:
  std::vector<std::unique_ptr<re2::RE2>> res;
  // set to max mem to 50M, details see re2.h
  static const size_t maxMem = 50000000;
};

} // namespace regexbench

#endif
