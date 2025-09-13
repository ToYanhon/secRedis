#pragma once
#include <string.h>

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "MutexGuard/MutexGuard.hpp"

namespace yanhon {

enum class LOG_LEVEL { ERROR, WARNING, INFO };

class Sink {
   public:
    virtual void write(const std::string& msg) = 0;
    virtual ~Sink() = 0;
};

class ConsoleSink : public Sink {
   public:
    void write(const std::string& msg) { std::cout << msg << std::endl; }
};

class FileSink : public Sink {
   public:
    FileSink(std::string file);
    ~FileSink();

    void write(const std::string& msg) override {
        if (!fs.is_open()) {
            std::cerr << "Error: Log file not open, writing to stderr instead: " << msg << std::endl;
            return;
        }
        fs << msg << std::endl;
        fs.flush();
    }

   private:
    std::fstream fs;
};

class LoggerManager {
   public:
    static LoggerManager& getInstance() {
        static LoggerManager instance;
        return instance;
    }
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    static unsigned long long ADD_CONSOLE_LOGGER() {
        return LoggerManager::getInstance().add(new ConsoleSink());
    }

    static unsigned long long ADD_FILE_LOGGER(const std::string& file) {
        return LoggerManager::getInstance().add(new FileSink(file));
    }

    static void REMOVE_LOGGER(unsigned long long id) {
        LoggerManager::getInstance().remove(id);
    }

    static void LOG_ERROR(const std::string& msg) {
        LoggerManager::getInstance().write(LOG_LEVEL::ERROR, msg, __FILE__,
                                           __LINE__);
    }

    static void LOG_WARNING(const std::string& msg) {
        LoggerManager::getInstance().write(LOG_LEVEL::WARNING, msg, __FILE__,
                                           __LINE__);
    }

    static void LOG_INFO(const std::string& msg) {
        LoggerManager::getInstance().write(LOG_LEVEL::INFO, msg, __FILE__,
                                           __LINE__);
    }

    unsigned long long add(Sink* sink);
    void remove(unsigned long long id);

    void write(LOG_LEVEL level, const std::string& msg,
               const std::string& file = __FILE__, int lines = __LINE__);

   private:
    LoggerManager();
    ~LoggerManager();

    std::unordered_map<unsigned long long, Sink*> vec;
    pthread_mutex_t map_mutex;

    unsigned long long id;  // when changed, mutex needed

    std::string format(LOG_LEVEL level, const std::string& msg,
                       const std::string& time, const std::string& file,
                       int lines);

    std::string getCurrTime();

    std::string levelToString(LOG_LEVEL level);
};

}  // namespace yanhon

#define ADD_CONSOLE_LOGGER() LoggerManager::ADD_CONSOLE_LOGGER()

#define ADD_FILE_LOGGER(file) LoggerManager::ADD_FILE_LOGGER(file)

#define LOG_ERROR(msg) LoggerManager::LOG_ERROR(msg)

#define LOG_WARNING(msg) LoggerManager::LOG_WARNING(msg)

#define LOG_INFO(msg) LoggerManager::LOG_INFO(msg)

#define REMOVE_LOGGER(n) LoggerManager::REMOVE_LOGGER(n)