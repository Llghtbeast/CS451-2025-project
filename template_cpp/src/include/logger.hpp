#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>

#include "globals.hpp"

class Logger {
public:
  explicit Logger(const std::string &path);
  ~Logger();

  void logBroadcast(msg_seq_t seq);
  void logDelivery(uint64_t sender_id, msg_seq_t seq);
  void logDecision(std::set<proposal_t>& proposals);
  
  void write();
  void flush();
  void cleanup();
  
  private:
  void enqueueLine(const std::string &line);

  std::ofstream log_file;
  std::vector<std::string> queue;
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic_bool running{false};
  std::thread worker;
};