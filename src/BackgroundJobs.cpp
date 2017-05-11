#include <cstring>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "BackgroundJobs.h"
#include "regexbench.h"

namespace regexbench {

BackgroundJobs::BackgroundJobs(const std::string& pipe, Engine* eng,
                               const std::string& rule, uint32_t cnt)
    : rule_file(rule), engine(eng), compile_cnt(cnt)
{
  if (!pipe.empty()) {
    int ret = mkfifo(pipe.c_str(), 0666);
    if (ret < 0 && errno != EEXIST) {
      std::cerr << "Error making pipe node " << pipe << std::endl;
      throw std::runtime_error("Error making pipe node");
    }
    fifo_fd = open(pipe.c_str(), O_RDWR); // use O_RDWR to prevent EOF loop
    if (fifo_fd < 0) {
      std::cerr << "Error opening fifo " << pipe << std::endl;
      throw std::runtime_error("Error opening fifo");
    }
    int flags = fcntl(fifo_fd, F_GETFL);
    fcntl(fifo_fd, F_SETFL, flags | O_NONBLOCK); // TODO : check return value
  }
}

void BackgroundJobs::start()
{
  run.store(true);
  if (compile_cnt)
    thrs.push_back(std::thread(&BackgroundJobs::compileTest, this));
  if (fifo_fd >= 0)
    thrs.push_back(std::thread(&BackgroundJobs::onlineUpdateTest, this));
}

void BackgroundJobs::stop()
{
  run.store(false);
  if (fifo_fd >= 0) {
    char buf[2] = ".";      // content doesn't matter
    write(fifo_fd, buf, 2); // to wake up onlineUpdateTest
  }
  for (auto& t : thrs)
    t.join();
}

void BackgroundJobs::compileTest()
{
  for (size_t cnt = 0; cnt < compile_cnt && run.load(); ++cnt) {
    std::cout << "Compile test # " << cnt << "..." << std::endl;
    engine->compile_test(loadRules(rule_file));
    std::cout << "done" << std::endl;
  }
}

void BackgroundJobs::doUpdate(const std::string& update_file)
{
  std::ifstream origIs(rule_file);
  std::ifstream updateIs(update_file);
  std::string combined_rule_file = "tempmerge.rule";
  std::ofstream combinedOs(combined_rule_file);

  combinedOs << origIs.rdbuf() << std::endl << updateIs.rdbuf();
  combinedOs.flush();
  combinedOs.close();

  engine->update_test(regexbench::loadRules(combined_rule_file));
}

void BackgroundJobs::onlineUpdateTest()
{
  fd_set read_fd_set;
  FD_ZERO(&read_fd_set);
  FD_SET(fifo_fd, &read_fd_set);
  int ret = 0;
  char buffer[MAX_PIPE_BUF];

  std::queue<std::string> updateRules;

  ssize_t total = 0;
  while (run.load()) {
    while (!updateRules.empty() && run.load()) {
      const auto& file = updateRules.front();
      doUpdate(file);
      // std::cout << "Update rule file is " << file << std::endl;
      updateRules.pop();
    }

    if (!run.load())
      break;

    ret = select(fifo_fd + 1, &read_fd_set, NULL, NULL, NULL);
    if (!run.load() || ret < 0) {
      // TODO : error logging (for ret < 0 case)
      break;
    }
    ssize_t nbytes = 0;
    while (total < MAX_PIPE_BUF &&
           (nbytes = read(fifo_fd, buffer + total,
                          static_cast<size_t>(MAX_PIPE_BUF - total))) > 0) {
      while (nbytes-- > 0) {
        char c = buffer[total++];
        if (c == '\x0d' || c == '\x0a') { // new line is a separator
          if (total - 1 > 0) {
            // std::string rule(buffer, total - 1);
            // std::cout << "Update rule file is : " << rule << std::endl;
            // updateRules.push(rule); // TODO
            updateRules.push(
                std::string(buffer, static_cast<size_t>(total - 1)));
          }
          if (nbytes > 0)
            std::memmove(buffer, buffer + total, static_cast<size_t>(nbytes));
          total = 0;
        }
      }
    }                   // read loop
    if (nbytes == -1) { // read returned error
      if (errno == EAGAIN) {
        // std::cout << "EAGAIN" << std::endl;
      } else {
        std::cout << "error occurred during reading update pipe" << std::endl;
        break;
      }
    }
    if (total == MAX_PIPE_BUF) {
      std::cout << "no available buffer for reading update pipe" << std::endl;
      break;
    }
  } // big loop (valid until run is true)
}

BackgroundJobs::~BackgroundJobs()
{
  if (fifo_fd >= 0) {
    close(fifo_fd);
  }
}

} // namespace regexbench
