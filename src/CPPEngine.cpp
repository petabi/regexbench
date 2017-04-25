#include <sstream>

#include "CPPEngine.h"

using namespace regexbench;

void CPPEngine::compile(const std::vector<Rule>& rules, size_t)
{
  std::stringstream msg;
  for (const auto& rule : rules) {
    std::regex_constants::syntax_option_type op{
        std::regex_constants::ECMAScript};

    if (rule.isSet(MOD_CASELESS)) {
      op |= std::regex_constants::icase;
    }

    try {
      res.emplace_back(std::make_unique<std::regex>(rule.getRegexp(), op));
    } catch (const std::exception& e) {
      msg << "id: " << rule.getID() << " " << e.what()
          << " rule:" << rule.getRegexp() << "\n";
      continue;
    }
  }
  if (msg.str().size()) {
    std::runtime_error error(msg.str());
    throw error;
  }
}

size_t CPPEngine::match(const char* data, size_t, size_t, size_t /*thr*/,
                        size_t* /*pId*/)
{
  std::cmatch m;
  for (const auto& re : res) {
    if (std::regex_search(data, m, *re))
      return 1;
  }
  return 0;
}
