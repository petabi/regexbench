#include <netinet/in.h>
#include <sys/types.h>

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include <sched.h>
#include <signal.h>
#ifdef __FreeBSD__
#include <sys/cpuset.h>
#endif
#include <sys/resource.h>
#include <sys/time.h>

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "Engine.h"
#include "Logger.h"
#include "PcapSource.h"
#include "Session.h"
#include "regexbench.h"

using namespace regexbench;

uint32_t regexbench::getPLOffset(const std::string& packet)
{
  uint16_t offset = 0;
  uint16_t ether_type =
      ntohs(reinterpret_cast<const ether_header*>(packet.data())->ether_type);
  if (ether_type == ETHERTYPE_VLAN) {
    offset = 4;
    ether_type =
        ntohs(reinterpret_cast<const ether_header*>(packet.data() + offset)
                  ->ether_type);
  }
  switch (ether_type) {
  case ETHERTYPE_IP: {
    offset += sizeof(ether_header);
    const ip* ih = reinterpret_cast<const ip*>(packet.data() + offset);
    offset += ih->ip_hl << 2;
    switch (ih->ip_p) {
    case IPPROTO_TCP:
      offset += reinterpret_cast<const tcphdr*>(packet.data() + offset)
#ifdef __linux__
                    ->doff
#else
                    ->th_off
#endif
                << 2;
      break;
    case IPPROTO_UDP:
      offset += sizeof(udphdr);
      break;
    case IPPROTO_ICMP:
      offset += ICMP_MINLEN;
      break;
    default:
      break;
    }
  } break;
  case ETHERTYPE_IPV6: {
    offset += sizeof(ether_header) + sizeof(ip6_hdr);
    const ip6_hdr* ih6 =
        reinterpret_cast<const ip6_hdr*>(packet.data() + offset);
    switch (ih6->ip6_ctlun.ip6_un1.ip6_un1_nxt) {
    case IPPROTO_TCP:
      offset += reinterpret_cast<const tcphdr*>(packet.data() + offset)
#ifdef __linux__
                    ->doff
#else
                    ->th_off
#endif
                << 2;
      break;
    case IPPROTO_UDP:
      offset += sizeof(udphdr);
      break;
    case IPPROTO_ICMP:
      offset += ICMP_MINLEN;
      break;
    default:
      break;
    }
  } break;
  default:
    break;
  }
  return offset;
}

std::vector<MatchMeta> regexbench::buildMatchMeta(const PcapSource& src,
                                                  size_t& nsessions)
{
  std::vector<MatchMeta> matcher_info;
  SessionTable sessionTable;
  for (const auto& pkt : src) {
    auto offset = getPLOffset(pkt);
    size_t sid = 0;
    Session s(pkt.data());
    sessionTable.find(s, sid);

    matcher_info.emplace_back(sid, offset, pkt.size() - offset);
  }
  nsessions = sessionTable.getSessionNum();
  return matcher_info;
}

void regexbench::compile_test_thread(const Engine* engine,
                                     const std::string rule_file,
                                     uint32_t compile_cnt)
{
  for (size_t cnt = 0; cnt < compile_cnt; ++cnt) {
    std::cout << "Compile test count " << cnt << std::endl;
    engine->compile_test(regexbench::loadRules(rule_file));
  }
}

std::mutex online_update_mtx;
std::condition_variable online_update_cv;
bool doUpdate = false;
bool reallyUpdate = false;

void regexbench::online_update_thread(Engine* engine,
                                      const std::string orig_file,
                                      const std::string update_file)
{
  std::unique_lock<std::mutex> lk(online_update_mtx);
  // TODO : maybe we should implement while loop if necessary
  online_update_cv.wait(lk, [] { return doUpdate; });

  // std::cout << "Online update thread signalled!!" << std::endl;
  if (!reallyUpdate) {
    // std::cout << "Online update terminating w/o updating" << std::endl;
    lk.unlock();
    return;
  }
  lk.unlock();

  std::ifstream origIs(orig_file);
  std::ifstream updateIs(update_file);
  std::string combined_rule_file = "tempmerge.rule";
  std::ofstream combinedOs(combined_rule_file);

  combinedOs << origIs.rdbuf() << updateIs.rdbuf();
  combinedOs.close();

  engine->update_test(regexbench::loadRules(combined_rule_file));
}

#if 0
MatchResult regexbench::match(Engine& engine, const PcapSource& src,
                              long repeat, const std::vector<MatchMeta>& meta)
{
  struct rusage begin, end;
  MatchResult result;
  getrusage(RUSAGE_SELF, &begin);
  for (long i = 0; i < repeat; ++i) {
    for (size_t j = 0; j < src.getNumberOfPackets(); j++) {
      auto matches =
          engine.match(src[j].data() + meta[j].oft, meta[j].len, meta[j].sid);
      if (matches) {
        result.nmatches += matches;
        result.nmatched_pkts++;
      }
    }
  }
  getrusage(RUSAGE_SELF, &end);
  timersub(&(end.ru_utime), &(begin.ru_utime), &result.udiff);
  timersub(&(end.ru_stime), &(begin.ru_stime), &result.sdiff);
  return result;
}
#else
#ifdef __linux__
using cpuset_t = cpu_set_t;
#endif
void regexbench::matchThread(Engine* engine, const PcapSource* src, long repeat,
                             size_t core, size_t sel,
                             const std::vector<MatchMeta>* meta,
                             MatchResult* result, Logger* logger)
{
#ifdef CPU_SET
  cpuset_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset_t), &cpuset) != 0) {
    std::cerr << "Setting affinty to a match thread failed" << std::endl;
    return;
  }
#endif

#ifdef RUSAGE_THREAD
  struct rusage begin, end;
  getrusage(RUSAGE_THREAD, &begin);
#endif
  for (long i = 0; i < repeat; ++i) {
    for (size_t j = 0; j < src->getNumberOfPackets(); j++) {
      size_t matchId;
      auto matches =
          engine->match((*src)[j].data() + (*meta)[j].oft, (*meta)[j].len,
                        (*meta)[j].sid, sel, &matchId);
      if (matches) {
        result->nmatches += matches;
        result->nmatched_pkts++;
        if (logger)
          logger->log("Thread ", sel, "(@", core, ") packet ", j,
                      " matches rule ", matchId);
      }
    }
  }
#ifdef RUSAGE_THREAD
  getrusage(RUSAGE_THREAD, &end);
  timersub(&(end.ru_utime), &(begin.ru_utime), &result->udiff);
  timersub(&(end.ru_stime), &(begin.ru_stime), &result->sdiff);
#endif
}
#endif

void regexbench::signal_update_thread(bool really_update)
{
  std::unique_lock<std::mutex> lk(online_update_mtx);
  doUpdate = true;
  reallyUpdate = really_update; // TODO this is ugly : revisit!
  lk.unlock();
  online_update_cv.notify_one();
}

static void sigusr1_handler(int /*sig*/)
{
  std::cout << "sigusr1" << std::endl;
  signal_update_thread(true);
}

std::vector<MatchResult> regexbench::match(Engine& engine,
                                           const PcapSource& src, long repeat,
                                           const std::vector<size_t>& cores,
                                           const std::vector<MatchMeta>& meta,
                                           const std::string& logfile)
{
  std::vector<std::thread> threads;
  std::vector<size_t>::const_iterator coreIter, coreEnd;
  std::vector<size_t> defaultCores;

  std::vector<MatchResult> results;
  if (cores.size() < 2) {
    defaultCores.push_back(0); // TODO : revisit
    defaultCores.push_back(0);
    results.resize(1);
    coreIter = defaultCores.cbegin();
    coreEnd = defaultCores.cend();
  } else {
    results.resize(cores.size() - 1);
    coreIter = cores.cbegin();
    coreEnd = cores.cend();
  }
  auto mainCore = *coreIter++;

#ifdef CPU_SET
  // set affinity to main thread itself
  cpuset_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(mainCore, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset_t), &cpuset) != 0) {
    std::cerr << "Setting affinty to a match thread failed" << std::endl;
    return std::vector<MatchResult>();
  }
#endif

  // signal handling for USR1
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = sigusr1_handler;
  sa.sa_flags = 0; /* clear SA_RESTART flag */
  sigaction(SIGUSR1, &sa, NULL);

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  // Every threads will inherit the following signal mask
  pthread_sigmask(SIG_BLOCK, &sigset, NULL);

  // logger setting
  std::unique_ptr<Logger> pLogger;

  if (!logfile.empty()) {
    pLogger.reset(new Logger(logfile));
    if (!pLogger->isOpen())
      pLogger.reset();
  }
  size_t i = 0;
  for (; coreIter != coreEnd; ++coreIter, ++i) {
    threads.push_back(std::thread(&regexbench::matchThread, &engine, &src,
                                  repeat, *coreIter, i, &meta, &results[i],
                                  (pLogger ? pLogger.get() : nullptr)));
  }

  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

  for (auto& thr : threads)
    thr.join();

  return results;
}

std::vector<regexbench::Rule> regexbench::loadRules(const std::string& filename)
{
  std::ifstream ruleifs(filename);
  if (!ruleifs) {
    std::cerr << "cannot open rule file: " << filename << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return regexbench::loadRules(ruleifs);
}
