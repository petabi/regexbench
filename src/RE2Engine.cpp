#include <sstream>

#include "RE2Engine.h"

using namespace regexbench;

void RE2Engine::compile(const std::vector<Rule>& rules, size_t)
{
  std::stringstream msg;
  for (const auto& rule : rules) {
    RE2::Options op;
    op.set_max_mem(maxMem);

    if (rule.isSet(MOD_CASELESS)) {
      op.set_case_sensitive(false);
    }
    if (rule.isSet(MOD_MULTILINE)) {
      op.set_one_line(false);
    }
    if (rule.isSet(MOD_DOTALL)) {
      op.set_dot_nl(true);
    }

    auto re = std::make_unique<re2::RE2>(rule.getRegexp(), op);

    if (re->ok()) {
      res.push_back(std::move(re));
    } else {
      msg << rule.getID() << " ";
    }
  }
  if (msg.str().size()) {
    std::runtime_error error(msg.str());
    throw error;
  }
}

size_t RE2Engine::match(const char* data, size_t len, size_t, size_t /*thr*/,
                        size_t* /*pId*/)
{
  len = 0;
  for (const auto& re : res) {
    if (re2::RE2::PartialMatch(data, *re)) {
      return 1;
    }
  }
  return 0;
}
