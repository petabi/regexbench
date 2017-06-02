#include "config.h"

#include <sys/resource.h>
#include <sys/time.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "BackgroundJobs.h"
#include "BoostEngine.h"
#include "CPPEngine.h"
#ifdef HAVE_HYPERSCAN
#include "HyperscanEngine.h"
#endif
#ifdef HAVE_PCRE2
#include "PCRE2Engine.h"
#endif
#include "PcapSource.h"
#ifdef HAVE_RE2
#include "RE2Engine.h"
#endif
#ifdef HAVE_REMATCH
#include "REmatchEngine.h"
#endif
#include "Logger.h"
#include "Rule.h"
#include "regexbench.h"

template <typename Derived, typename Base, typename Del>
std::unique_ptr<Derived, Del>
static_unique_ptr_cast(std::unique_ptr<Base, Del>&& p)
{
  auto d = static_cast<Derived*>(p.release());
  return std::unique_ptr<Derived, Del>(d, std::move(p.get_deleter()));
}

static bool endsWith(const std::string&, const char*);

Arguments regexbench::init(const std::string& rule_file,
                           const std::string& pcap_file,
                           const std::string& output_file,
                           const EngineType& engine, uint32_t nthreads,
                           const std::string& affinity, int32_t repeat)
{
  Arguments args;

  args.rule_file = rule_file;
  args.pcap_file = pcap_file;
  args.output_file = output_file;
  args.engine = engine;
  args.repeat = repeat;
  args.pcre2_concat = 0;
  args.rematch_session = 0;
  args.compile_test = 0;
  args.num_threads = nthreads;
  args.update_pipe = "";
  args.log_file = "";
  args.quiet = true;
  args.nmatch = 0;
  args.cores = setup_affinity(nthreads, affinity);

  return args;
}

int regexbench::exec(Arguments &args)
{
  try {
    std::string prefix;
    std::unique_ptr<regexbench::Engine> engine;
    size_t nsessions = 0;
    regexbench::PcapSource pcap(args.pcap_file);
    auto match_info = buildMatchMeta(pcap, nsessions);

    struct rusage compileBegin, compileEnd;
    getrusage(RUSAGE_SELF, &compileBegin);
    switch (args.engine) {
    case EngineType::boost:
      prefix = "boost";
      engine = std::make_unique<regexbench::BoostEngine>();
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
    case EngineType::std_regex:
      engine = std::make_unique<regexbench::CPPEngine>();
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
#ifdef HAVE_HYPERSCAN
    case EngineType::hyperscan:
      prefix = "hyperscan";
      if (args.rematch_session) {
        engine = std::make_unique<regexbench::HyperscanEngineStream>();
        engine->init(nsessions);
      } else {
        engine = std::make_unique<regexbench::HyperscanEngine>();
      }
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
#endif
#ifdef HAVE_PCRE2
    case EngineType::pcre2:
      prefix = "pcre2";
      engine = std::make_unique<regexbench::PCRE2Engine>();
      engine->init(args.pcre2_concat);
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
    case EngineType::pcre2_jit:
      engine = std::make_unique<regexbench::PCRE2JITEngine>();
      engine->init(args.pcre2_concat);
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
#endif
#ifdef HAVE_RE2
    case EngineType::re2:
      prefix = "re2";
      engine = std::make_unique<regexbench::RE2Engine>();
      engine->compile(regexbench::loadRules(args.rule_file), args.num_threads);
      break;
#endif
#ifdef HAVE_REMATCH
    case EngineType::rematch:
      prefix = "rematch";
      if (args.rematch_session) {
#ifdef WITH_SESSION
        engine = std::make_unique<regexbench::REmatchAutomataEngineSession>(
            args.nmatch);
        engine->compile(regexbench::loadRules(args.rule_file),
                        args.num_threads);
#endif
      } else if (endsWith(args.rule_file, ".nfa")) {
        engine =
            std::make_unique<regexbench::REmatchAutomataEngine>(args.nmatch);
        engine->load(args.rule_file, args.num_threads);
      } else if (endsWith(args.rule_file, ".so")) {
        engine = std::make_unique<regexbench::REmatchSOEngine>();
        engine->load(args.rule_file, args.num_threads);
      } else {
        engine = std::make_unique<regexbench::REmatchAutomataEngine>(
            args.nmatch, args.reduce);
        engine->compile(regexbench::loadRules(args.rule_file),
                        args.num_threads);
      }
      engine->init(nsessions);
      break;
    case EngineType::rematch2:
      prefix = "rematch2";
      if (endsWith(args.rule_file, ".nfa")) {
        engine =
            std::make_unique<regexbench::REmatch2AutomataEngine>(args.nmatch);
        engine->load(args.rule_file, args.num_threads);
      } else {
        engine = std::make_unique<regexbench::REmatch2AutomataEngine>(
            args.nmatch, args.reduce
#ifdef USE_TURBO
            ,
            args.turbo
#endif
            );
        engine->compile(regexbench::loadRules(args.rule_file),
                        args.num_threads);
      }
      break;
#endif
    }
    getrusage(RUSAGE_SELF, &compileEnd);
    args.compile_time = compileReport(compileBegin, compileEnd, pcap, args.quiet);

    // set up background jobs
    using BGJ = regexbench::BackgroundJobs;
    BGJ bgj(args.update_pipe, engine.get(), args.rule_file, args.compile_test);
    regexbench::setAffinity(args.cores[0], "background");
    bgj.start(); // launch background jobs (to actually run or not will be
                 // determined inside class instance)

    std::vector<regexbench::MatchResult> results = match(
        *engine, pcap, args.repeat, args.cores, match_info, args.log_file);

    report(prefix, pcap, args, results);

    bgj.stop();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

bool endsWith(const std::string& obj, const char* end)
{
  auto r = obj.rfind(end);
  if ((r != std::string::npos) && (r == obj.size() - std::strlen(end)))
    return true;
  return false;
}

std::vector<size_t> setup_affinity(size_t num, const std::string& arg)
{
  auto ncpus = std::thread::hardware_concurrency();

  // num : user specified number of threads of matchers
  //   (main thread is not included here)
  // arg : user specified description of core assignment
  //   (comma separated decimals w/o space in between)

  num += 1;    // main thread included
  int inc = 1; // automatic increase value

  // firstly check if there's trailing ":[decimal's]"
  auto pos = arg.find(":");
  if (pos != std::string::npos) {
    std::string incStr = arg.substr(pos + 1);
    if (incStr.empty())
      inc = 0;
    try {
      inc = stoi(incStr);
    } catch (const std::exception&) {
    }
  }

  std::string csv = arg.substr(0, pos);

  // parse comma separated input
  std::replace(csv.begin(), csv.end(), ',', ' ');
  std::istringstream is(csv);

  std::vector<size_t> cores(num); // to return
  int maxCore = static_cast<int>(ncpus - 1);
  try {
    std::istream_iterator<int> iter = std::istream_iterator<int>(is);
    int last = 0;
    std::generate(cores.begin(), cores.end(), [&iter, &last, inc, maxCore]() {
      int core = 0;
      if (iter != std::istream_iterator<int>()) {
        core = std::min(std::max(*iter, 0), maxCore);
        ++iter;
      } else
        core = std::min(std::max(last + inc, 0), maxCore);
      last = core;
      return static_cast<size_t>(core);
    });
  } catch (const std::exception&) {
    // some formatting error
    std::cerr << "User provided affinity assignment format error" << std::endl;
    // go with default assignment scheme
    for (int i = 0; i < static_cast<int>(num); ++i)
      cores[static_cast<size_t>(i)] =
          static_cast<size_t>((i > maxCore) ? maxCore : i);
  }
  return cores;
}
