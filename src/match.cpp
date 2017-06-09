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

#include <atomic>
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

#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD RUSAGE_SELF
#endif

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

#ifdef __linux__
using cpuset_t = cpu_set_t;
#endif
int regexbench::setAffinity(size_t core, const std::string& thrName)
{
#ifdef CPU_SET
  // set affinity to main thread itself
  cpuset_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset_t), &cpuset) != 0) {
    if (!thrName.empty()) {
      std::cerr << "Setting affinty to " << thrName << " thread failed"
                << std::endl;
    }
    return -1;
  }
#endif
  return 0;
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

void regexbench::matchThread(Engine* engine, const PcapSource* src, long repeat,
                             size_t core, size_t sel,
                             const std::vector<MatchMeta>* meta,
                             MatchResult* result, Logger* logger)
{
  setAffinity(core, "match");

  struct rusage begin, end;
  getrusage(RUSAGE_THREAD, &begin);
  for (long i = 0; i < repeat; ++i) {
    for (size_t j = 0; j < src->getNumberOfPackets(); j++) {
      size_t matchId;
      auto matches =
          engine->match((*src)[j].data() + (*meta)[j].oft, (*meta)[j].len,
                        (*meta)[j].sid, sel, &matchId);
      if (matches) {
        result->cur.nmatches += matches;
        result->cur.nmatched_pkts++;
        if (logger)
          logger->log("Thread ", sel, "(@", core, ") packet ", j,
                      " matches rule ", matchId);
      }
      result->cur.nbytes += (*src)[j].length();
      result->cur.npkts++;
    }
  }
  getrusage(RUSAGE_THREAD, &end);
  timersub(&(end.ru_utime), &(begin.ru_utime), &result->udiff);
  timersub(&(end.ru_stime), &(begin.ru_stime), &result->sdiff);
  result->stop.store(true);
}

std::vector<MatchResult> regexbench::match(Engine& engine,
                                           const PcapSource& src, long repeat,
                                           const std::vector<size_t>& cores,
                                           const std::vector<MatchMeta>& meta,
                                           const std::string& logfile,
                                           realtimeFunc func)
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

  setAffinity(mainCore, "main");

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

  uint32_t sec = 0;
  bool realTime = true;

  while (realTime) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    sec++;
    statistic(sec, results, func);

    for (const auto& result : results) {
      if (!result.stop.load()) {
        realTime = true;
        break;
      } else
        realTime = false;
    }
  }

  sec++;
  statistic(sec, results, func);

  statistic(0, results, func);

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
