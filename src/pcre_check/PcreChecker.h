#ifndef PCRE_CHECKER_H
#define PCRE_CHECKER_H

#include <map>
#include <string>
#include <vector>

#include "../Rule.h"
#include "db_setup.h"

struct AuxInfo {
  int resMatchId;
  int resNomatchId;
  int resErrorId;
  std::map<std::string, int> str2EngineId;
  uint32_t nmatch; // this is rematch only parameter
  uint32_t single; // single test mode (used to indicate match result also)
  int result;

  std::vector<regexbench::Rule> rules;
  std::string data; // used for singletest mode
};

void checkRematch(PcreCheckDb& db, struct AuxInfo& aux);
void checkHyperscan(PcreCheckDb& db, struct AuxInfo& aux);
void checkPcre(PcreCheckDb& db, struct AuxInfo& aux);

#endif
