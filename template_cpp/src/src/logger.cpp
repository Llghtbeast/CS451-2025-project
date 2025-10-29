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
void Logger::logDelivery(uint64_t sender_id, uint32_t seq)
{
  std::ostringstream os;
  os << "d " << sender_id << " " << seq;
  enqueueLine(os.str());
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
 * Writes enqueued log lines to the log file.
 */
void Logger::write()
{
  std::vector<std::string> local;
  // swap shared queue into local buffer under lock
  {
    std::lock_guard<std::mutex> lk(mutex);
    local.swap(queue);
  }

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