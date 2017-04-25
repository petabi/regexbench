#ifndef REGEXBENCH_LOGGER_H
#define REGEXBENCH_LOGGER_H

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

using lock_guard = std::lock_guard<std::mutex>;

class Logger {
public:
  Logger(const std::string fname) : file(fname), opened(file.is_open() ? 1 : 0)
  {
  }

  template <typename Arg, typename... Args> void log(Arg&& arg, Args&&... args)
  {
    if (!opened)
      return;
    lock_guard lock(mtx);
    file << std::forward<Arg>(arg);

    // initializer list trick
    using expander = int[];
    (void)expander{0, (void(file << std::forward<Args>(args)), 0)...};
    file << std::endl;
  }

  template <typename Arg> void log(Arg&& arg)
  {
    if (!opened)
      return;
    lock_guard lock(mtx);
    file << std::forward<Arg>(arg);
    file << std::endl;
  }

  bool isOpen() { return opened > 0; }

private:
  std::ofstream file;
  std::mutex mtx;
  size_t opened = 0;
};

#endif
