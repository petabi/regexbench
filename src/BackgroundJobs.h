// -*- c++ -*-
#ifndef FIFO_CHAN_H
#define FIFO_CHAN_H

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "Engine.h"

namespace regexbench {

class BackgroundJobs {
public:
  BackgroundJobs(const std::string& pipe, Engine* eng, const std::string& rule,
                 uint32_t compile_cnt);
  ~BackgroundJobs();

  void start();
  void stop();

private:
  // thread functions
  void compileTest();
  void onlineUpdateTest();

  // online update worker
  void doUpdate(const std::string& update_file);

  static constexpr ssize_t MAX_PIPE_BUF = 128;

  std::vector<std::thread> thrs;
  std::atomic_bool run{false};

protected:
  char __padding[7];

private:
  std::string rule_file;
  Engine* engine;
  int fifo_fd = -1;
  uint32_t compile_cnt = 0;
};
} // namespace regexbench

#endif // FIFO_CHAN_H
