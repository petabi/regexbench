// -*- c++ -*-
#ifndef REGEXBENCH_PCRE2_H
#define REGEXBENCH_PCRE2_H

#include <memory>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "Engine.h"

namespace regexbench {

class PCRE2Engine : public Engine {
public:
  PCRE2Engine() = default;
  virtual ~PCRE2Engine() = default;

  virtual void compile(const std::vector<Rule>&, size_t);
  virtual void init(size_t);
  virtual size_t match(const char*, size_t, size_t, size_t = 0,
                       size_t* = nullptr);

protected:
  struct PCRE2_DATA {
    pcre2_code* re;
    pcre2_match_data* mdata;
    PCRE2_DATA(pcre2_code* re_, pcre2_match_data* mdata_)
        : re{re_}, mdata{mdata_}
    {
    }
    ~PCRE2_DATA()
    {
      pcre2_code_free(re);
      pcre2_match_data_free(mdata);
    }
    void compileRule();
  };

  uint32_t buildRuleOffset(std::vector<Rule>&);
  void binaryCompile(std::string, size_t, size_t, uint32_t);
  std::vector<std::unique_ptr<PCRE2_DATA>> res;
  std::vector<size_t> ruleOffset;
  size_t isConcat;
};

class PCRE2JITEngine : public PCRE2Engine {
public:
  PCRE2JITEngine() = default;
  virtual ~PCRE2JITEngine() = default;

  virtual void compile(const std::vector<Rule>&, size_t);
};

} // namespace regexbench

#endif
