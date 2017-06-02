#include "config.h"

#include <boost/program_options.hpp>
#include <thread>

#include "regexbench.h"

namespace po = boost::program_options;
using namespace regexbench;

static Arguments parse_options(int argc, const char* argv[]);

int main(int argc, const char* argv[])
{
  auto args = parse_options(argc, argv);

  return regexbench::exec(args);
}

Arguments parse_options(int argc, const char* argv[])
{
  Arguments args;
  std::string engine;
  std::string prefix;
  std::string affinity;

  po::options_description posargs;
  posargs.add_options()("rule_file", po::value<std::string>(&args.rule_file),
                        "Rule (regular expression) file name");
  posargs.add_options()("pcap_file", po::value<std::string>(&args.pcap_file),
                        "pcap file name");
  po::positional_options_description positions;
  positions.add("rule_file", 1).add("pcap_file", 1);

  po::options_description optargs("Options");
  optargs.add_options()("help,h", "Print usage information.");
  optargs.add_options()(
      "engine,e", po::value<std::string>(&engine)->default_value("hyperscan"),
      "Matching engine to run.");
  optargs.add_options()("repeat,r",
                        po::value<int32_t>(&args.repeat)->default_value(1),
                        "Repeat pcap multiple times.");
  optargs.add_options()(
      "concat,c", po::value<uint32_t>(&args.pcre2_concat)->default_value(0),
      "Concatenate PCRE2 rules.");
  optargs.add_options()(
      "session,s", po::value<uint32_t>(&args.rematch_session)->default_value(0),
      "Rematch session mode.");
  optargs.add_options()("prefix,p",
                        po::value<std::string>(&prefix)->default_value(""),
                        "Prefix to output json file name");
  optargs.add_options()(
      "output,o", po::value<std::string>(&args.output_file)->default_value(""),
      "Output JSON file.");
  optargs.add_options()(
      "logfile,l", po::value<std::string>(&args.log_file)->default_value(""),
      "Log file.");
  optargs.add_options()(
      "threads,n", po::value<uint32_t>(&args.num_threads)->default_value(1),
      "Number of threads.");
  optargs.add_options()("affinity,a",
                        po::value<std::string>(&affinity)->default_value("0"),
                        "Core affinity assignment (starting from main thread)");
  optargs.add_options()("reduce,R",
                        po::value<bool>(&args.reduce)->default_value(false),
                        "Use REduce with REmatch, default is false");
  optargs.add_options()(
      "compile,t", po::value<uint32_t>(&args.compile_test)->default_value(0),
      "Compile test");
  optargs.add_options()(
      "update,u", po::value<std::string>(&args.update_pipe)->default_value(""),
      "Pipe for signaling online update");
#ifdef USE_TURBO
  optargs.add_options()("turbo", "Turbo processing mode for rematch2");
#endif
  optargs.add_options()("match_num,m",
                        po::value<uint32_t>(&args.nmatch)->default_value(0),
                        "Match number");
  optargs.add_options()("quiet,q",
                        po::value<bool>(&args.quiet)->default_value(false),
                        "Quiet mode");
  po::options_description cliargs;
  cliargs.add(posargs).add(optargs);
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .options(cliargs)
                .positional(positions)
                .run(),
            vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << "Usage: regexbench <rule_file> <pcap_file>" << std::endl;
    std::cout << posargs << "\n" << optargs << "\n";
    std::exit(EXIT_SUCCESS);
  }

#ifdef USE_TURBO
  if (vm.count("turbo"))
    args.turbo = true;
#endif

  if (engine == "boost")
    args.engine = EngineType::boost;
  else if (engine == "cpp")
    args.engine = EngineType::std_regex;
#ifdef HAVE_HYPERSCAN
  else if (engine == "hyperscan")
    args.engine = EngineType::hyperscan;
#endif
#ifdef HAVE_PCRE2
  else if (engine == "pcre2")
    args.engine = EngineType::pcre2;
  else if (engine == "pcre2jit")
    args.engine = EngineType::pcre2_jit;
#endif
#ifdef HAVE_RE2
  else if (engine == "re2")
    args.engine = EngineType::re2;
#endif
#ifdef HAVE_REMATCH
  else if (engine == "rematch")
    args.engine = EngineType::rematch;
  else if (engine == "rematch2")
    args.engine = EngineType::rematch2;
#endif
  else {
    std::cerr << "unknown engine: " << engine << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (args.repeat <= 0) {
    std::cerr << "invalid repeat value: " << args.repeat << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (args.num_threads < 1) {
    std::cerr << "invalid number of threads: " << args.num_threads << std::endl;
    std::cerr << " (should be >= 1 .. overriding it to 1" << std::endl;
    args.num_threads = 1;
  }

  if (!args.quiet) {
    std::cout << "number of threads : " << args.num_threads << std::endl;
    args.cores = setup_affinity(args.num_threads, affinity);
    std::cout << "affinity setup is ..." << std::endl;
    for (auto core : args.cores)
      std::cout << " " << core;
    std::cout << std::endl;
  } else
    args.cores = setup_affinity(args.num_threads, affinity);

#ifndef WITH_SESSION
  if ((engine == "rematch" || engine == "rematch2") && args.rematch_session) {
    std::cerr << "not supporting session mode for now" << std::endl;
    args.rematch_session = 0;
  }
#endif

  if (args.output_file.empty()) {
    std::string name = prefix;
    if (!name.empty())
      name += "-";
    name += engine + "-";
    auto basename = args.rule_file;
    auto pos = basename.find_last_of("/\\");
    basename = basename.substr((pos == std::string::npos) ? 0 : pos + 1);
    name += basename + "-"; // rule
    basename = args.pcap_file;
    pos = basename.find_last_of("/\\");
    basename = basename.substr((pos == std::string::npos) ? 0 : pos + 3);
    name += basename + "-";                               // pcap
    name += "N" + std::to_string(args.num_threads) + "-"; // num threads
    name += "R" + std::to_string(args.repeat) + ".json";  // repeat

    args.output_file = name;
    if (!args.quiet) {
      std::cout << "Output file name is " << args.output_file << std::endl;
    }
  }

  if (!vm.count("rule_file")) {
    std::cerr << "error: no rule file" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (!vm.count("pcap_file")) {
    std::cerr << "error: no pcap file" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return args;
}
