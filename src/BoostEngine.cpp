#include <sstream>

#include "BoostEngine.h"

using namespace regexbench;

void BoostEngine::compile(const std::vector<Rule>& rules, size_t)
{
  std::stringstream msg;
  for (const auto& rule : rules) {
    boost::regex_constants::syntax_option_type op =
        boost::regex_constants::optimize;

    if (rule.isSet(MOD_CASELESS)) {
      op |= boost::regex_constants::icase;
    }
    if (rule.isSet(MOD_DOTALL)) {
      op |= boost::regex_constants::mod_s;
    }

    try {
      res.emplace_back(std::make_unique<boost::regex>(rule.getRegexp()));
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

size_t BoostEngine::match(const char* data, size_t, size_t, size_t /*thr*/,
                          size_t* /*pId*/)
{
  boost::cmatch m;
  for (const auto& re : res) {
    if (boost::regex_search(data, m, *re))
      return 1;
  }
  return 0;
}
