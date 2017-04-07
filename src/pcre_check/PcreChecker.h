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

  std::vector<regexbench::Rule> rules;
};

void checkRematch(PcreCheckDb& db, const struct AuxInfo& aux);
void checkHyperscan(PcreCheckDb& db, struct AuxInfo& aux);
void checkPcre(PcreCheckDb& db, struct AuxInfo& aux);

#endif
