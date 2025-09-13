#include "Logger/Logger.hpp"

#include <ctime>
#include <stdexcept>

namespace yanhon {

Sink::~Sink() {}

FileSink::FileSink(std::string file) {
  fs.open(file, std::ios::out | std::ios::app);
  if (!fs.is_open()) {
    std::cerr << "Error: Cannot open log file: " << file << std::endl;
  }
}

FileSink::~FileSink() {
  if (fs.is_open()) {
    fs.close();
  }
}

LoggerManager::LoggerManager() : id(1) {
  pthread_mutex_init(&map_mutex, nullptr);
}

LoggerManager::~LoggerManager() {
  {
    MutexGuard mg(map_mutex);
    for (auto &[_, sink] : vec) {
      delete sink;
    }
    vec.clear();
  }

  pthread_mutex_destroy(&map_mutex);
}

unsigned long long LoggerManager::add(Sink *sink) {
  if (sink != nullptr) {
    MutexGuard mg(map_mutex);
    vec[id] = sink;
    return id++;
  }
  throw std::runtime_error("no such logic");
}

void LoggerManager::remove(unsigned long long id) {
  if (vec.count(id)) {
    MutexGuard mg(map_mutex);
    vec.erase(id);
  }
}

void LoggerManager::write(LOG_LEVEL level, const std::string &msg,
                          const std::string &file, int lines) {
  std::string curr_time = getCurrTime();
  std::string short_file = file;
  size_t pos = file.find("server");
  if (pos != std::string::npos) {
    short_file = file.substr(pos); // 截取 server 及后面
  }
  std::string format_msg = format(level, msg, curr_time, short_file, lines);

  MutexGuard mg(map_mutex);
  for (auto &[_, sink] : vec) {
    sink->write(format_msg);
  }
}

std::string LoggerManager::format(LOG_LEVEL level, const std::string &msg,
                                  const std::string &time,
                                  const std::string &file, int lines) {
  return "[" + time + "] [" + std::string(levelToString(level)) + "] " + file +
         ":" + std::to_string(lines) + " " + msg;
}

std::string LoggerManager::getCurrTime() {
  time_t now = time(0);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
  return buf;
}

std::string LoggerManager::levelToString(LOG_LEVEL level) {
  std::unordered_map<LOG_LEVEL, std::string> level_map = {
      {LOG_LEVEL::ERROR, "ERROR"},
      {LOG_LEVEL::WARNING, "WARNING"},
      {LOG_LEVEL::INFO, "INFO"}};
  return level_map[level];
}

} // namespace yanhon