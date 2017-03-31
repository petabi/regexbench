#ifndef DB_SETUP_H
#define DB_SETUP_H

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

#endif
