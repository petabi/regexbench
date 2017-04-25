// -*- c++ -*-
#ifndef REGEXBENCH_HYPERSCANENGINE_H
#define REGEXBENCH_HYPERSCANENGINE_H

#include <memory>

#include <hs/hs_compile.h>
#include <hs/hs_runtime.h>

#include "Engine.h"

namespace regexbench {

class HyperscanEngine : public Engine {
public:
  HyperscanEngine();
  HyperscanEngine(const HyperscanEngine&) = delete;
  HyperscanEngine(HyperscanEngine&& o) : db(o.db), scratches(o.scratches)
  {
    o.db = nullptr;
    o.scratches.clear();
  }
  virtual ~HyperscanEngine();
  HyperscanEngine& operator=(const HyperscanEngine&) = delete;
  HyperscanEngine& operator=(HyperscanEngine&& o)
  {
    hs_free_database(db);
    db = o.db;
    o.db = nullptr;
    for (auto scratch : scratches)
      hs_free_scratch(scratch);
    scratches = o.scratches;
    o.scratches.clear();
    return *this;
  }

  virtual void compile(const std::vector<Rule>&, size_t = 1);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

protected:
  void reportFailedRules(const std::vector<Rule>&);
  hs_database_t* db;
  std::vector<hs_scratch_t*> scratches;
  hs_platform_info_t platform;
  size_t nsessions;
};

class HyperscanEngineStream : public HyperscanEngine {
public:
  HyperscanEngineStream() = default;
  virtual ~HyperscanEngineStream();
  virtual void init(size_t);

  void compile(const std::vector<Rule>&, size_t = 1);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

private:
  std::unique_ptr<hs_stream* []> streams;
};

} // namespace regexbench

#endif
