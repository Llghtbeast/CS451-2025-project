#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>

class Logger {
public:
  explicit Logger(const std::string &path);
  ~Logger();

  void logBroadcast(uint32_t seq);
  void logDelivery(uint32_t sender_id, uint32_t seq);
  
  void start();
  void flush();
  void cleanup();
  
  private:
  void enqueueLine(const std::string &line);
  void workerLoop();
  void writeLog(std::vector<std::string> &local, std::unique_lock<std::mutex> &lk);

  std::ofstream log_file;
  std::vector<std::string> queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic_bool running{false};
  std::thread worker;
};