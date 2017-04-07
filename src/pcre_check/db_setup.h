#ifndef DB_SETUP_H
#define DB_SETUP_H

#include <string>

#include <jsoncpp/json/json.h>
#include <litesql.hpp>
#include "pcrecheckdb.hpp"

// pcre_check namespace aliases
using pcre_check::PcreCheckDb;
using DbRule = pcre_check::Rule;
using pcre_check::Pattern;
using pcre_check::Grammar;
using pcre_check::Engine;
using pcre_check::Result;
using pcre_check::Test;
using pcre_check::TestGrammar;
using pcre_check::TestResult;

// litesql namespace aliases
using litesql::select;
using litesql::Blob;
using litesql::Except;
using litesql::NotFound;

// currently engines, results
template <typename T>
void parseNameList(PcreCheckDb& db, const std::string& member,
                   const Json::Value& root)
{
  if (root[member].empty() || !root[member].isArray())
    return;

  for (const auto& e : root[member]) {
    if (!e.isString())
      continue;
    try {
      select<T>(db, T::Name == e.asString()).one();
    } catch (NotFound) {
      T t(db);
      t.name = e.asString();
      t.update();
    }
  }
}

void parseRules(PcreCheckDb& db, const Json::Value&);
void parseGrammars(PcreCheckDb& db, const Json::Value&);
void parsePatterns(PcreCheckDb& db, const Json::Value&);
void parseTests(PcreCheckDb& db, const Json::Value&);
#endif
