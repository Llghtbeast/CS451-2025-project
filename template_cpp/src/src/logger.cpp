#include "logger.hpp"
#include <chrono>

/**
 * Constructor to initialize the Logger with the specified log file path.
 * @param path The path to the log file.
 */
Logger::Logger(const std::string &path)
{
  log_file.open(path, std::ios::out | std::ios::app);
  if (!log_file.is_open()) {
    throw std::runtime_error("Logger: failed to open log file: " + path);
  }
}

/**
 * Destructor to clean up resources used by the Logger.
 */
Logger::~Logger()
{
  cleanup();
}

/**
 * Logs a broadcast event with the specified sequence number.
 * @param seq The sequence number of the broadcasted message.
 */
void Logger::logBroadcast(uint32_t seq)
{
  std::ostringstream os;
  os << "b " << seq;
  enqueueLine(os.str());
}

/**
 * Logs a delivery event with the specified sender ID and sequence number.
 * @param sender_id The ID of the sender.
 * @param seq The sequence number of the delivered message.
 */
void Logger::logDelivery(uint32_t sender_id, uint32_t seq)
{
  std::ostringstream os;
  os << "d " << sender_id << " " << seq;
  enqueueLine(os.str());
}

/**
 * Starts the logger's worker loop
 */
void Logger::start()
{
  // Start running worker loop
  running.store(true);
  workerLoop();
}

/**
 * Enqueues a log line to be written to the log file.
 * @param line The log line to enqueue.
 */
void Logger::enqueueLine(const std::string &line)
{
  std::lock_guard<std::mutex> lk(mutex);
  queue.push_back(line);
}

/**
 * Flushes the log file by writing all enqueued log lines.
 */
void Logger::flush()
{
  std::vector<std::string> local;
  std::unique_lock<std::mutex> lk(mutex);

  // Write enqueued lines to log file
  writeLog(local, lk);
  
  // ensure file is flushed
  if (log_file.is_open()) log_file.flush();
}

/**
 * Cleans up resources used by the Logger.
 */
void Logger::cleanup()
{
  running.store(false);
  log_file.close();
}

/**
 * Worker loop that continuously writes enqueued log lines to the log file while running.
 */
void Logger::workerLoop()
{
  std::vector<std::string> local;
  std::unique_lock<std::mutex> lk {mutex, std::defer_lock};

  while (running.load()) {
    // Write enqueued lines to log file
    writeLog(local, lk);
    
    // Wait. Periodically wake up to check if lines are enqueued to be logged.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

/**
 * Writes enqueued log lines to the log file.
 * @param local A local buffer to swap with the shared queue.
 * @param lk A unique lock for synchronizing access to the shared queue.
 */
void Logger::writeLog(std::vector<std::string> &local, std::unique_lock<std::mutex> &lk)
{
  // swap shared queue into local buffer under lock
  lk.lock();
  local.swap(queue);
  lk.unlock();

  if (!local.empty() && log_file.is_open()) {
    // accumulate into one string to minimize syscalls
    std::string out;
    size_t approx = 0;
    for (auto &s : local) approx += s.size() + 1;
    out.reserve(approx);
    for (auto &s : local) {
      out += s;
      out.push_back('\n');
    }

    log_file.write(out.data(), static_cast<std::streamsize>(out.size()));
  }
}