#pragma once
#include <atomic>

#include "ThreadPool/ThreadPool.hpp"
#include "db/db.hpp"
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace yanhon {

struct ClientBuffer {
  std::vector<char> inBuffer; // 输入缓冲区
  size_t inReadPos = 0;       // 已解析位置
  size_t inWritePos = 0;      // 已写入位置
  size_t expectedLength = 0;  // 期望消息长度

  std::vector<char> outBuffer; // 输出缓冲区
  size_t outReadPos = 0;       // 已发送位置

  static constexpr size_t kInitialSize = 4096;

  mutable std::shared_mutex rwMutex; // 读写锁支持多读一写

  ClientBuffer(size_t initialSize = kInitialSize) {
    inBuffer.resize(initialSize);
    outBuffer.reserve(initialSize); // 输出缓冲区预分配空间
  }

  // 输入缓冲区可读字节数
  size_t readableBytes() const {
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    return inWritePos - inReadPos;
  }

  // 获取可读数据指针（线程安全）
  const char *peek() const {
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    return inBuffer.data() + inReadPos;
  }

  // 消费输入缓冲区数据
  void retrieve(size_t n) {
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    inReadPos += n;

    // 当大部分数据已消费时，重置缓冲区
    if (inReadPos > inBuffer.size() / 2) {
      size_t remaining = inWritePos - inReadPos;
      if (remaining > 0) {
        memmove(inBuffer.data(), inBuffer.data() + inReadPos, remaining);
      }
      inReadPos = 0;
      inWritePos = remaining;

      // 如果缓冲区过大则收缩
      if (inBuffer.capacity() > 4 * kInitialSize && remaining < kInitialSize) {
        std::vector<char>(inBuffer.begin(), inBuffer.begin() + remaining)
            .swap(inBuffer);
      }
    }
  }

  // 写入数据到输入缓冲区
  void append(const char *src, size_t n) {
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    ensureWritable(n);
    memcpy(inBuffer.data() + inWritePos, src, n);
    inWritePos += n;
  }

  // 确保输入缓冲区有足够空间
  void ensureWritable(size_t n) {
    if (inWritePos + n > inBuffer.size()) {
      // 尝试回收前面已读空间
      if (inReadPos > 0) {
        size_t readable = inWritePos - inReadPos;
        if (readable > 0) {
          memmove(inBuffer.data(), inBuffer.data() + inReadPos, readable);
        }
        inWritePos = readable;
        inReadPos = 0;
      }
      // 如果仍然不足则扩容
      if (inWritePos + n > inBuffer.size()) {
        inBuffer.resize(std::max(inWritePos + n, inBuffer.size() * 2));
      }
    }
  }

  // 输出缓冲区相关操作
  void appendOut(const char *src, size_t n) {
    // std::unique_lock<std::shared_mutex> lock(rwMutex);
    // // 直接追加到输出缓冲区，利用vector的扩容策略
    // outBuffer.insert(outBuffer.end(), src, src + n);

    // 优化版本：预分配空间减少扩容次数
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    size_t currentSize = outBuffer.size();
    size_t requiredSize = currentSize + n;

    if (requiredSize > outBuffer.capacity()) {
      // 预分配更多空间减少扩容次数
      outBuffer.reserve(std::max(requiredSize, outBuffer.capacity() * 2));
    }

    outBuffer.resize(requiredSize);
    memcpy(outBuffer.data() + currentSize, src, n);
  }

  void retrieveOut(size_t n) {
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    outReadPos += n;

    // 当大部分数据已发送时，重置输出缓冲区
    if (outReadPos > outBuffer.size() / 2) {
      outBuffer.erase(outBuffer.begin(), outBuffer.begin() + outReadPos);
      outReadPos = 0;

      // 收缩过大的缓冲区
      if (outBuffer.capacity() > 4 * kInitialSize &&
          outBuffer.size() < kInitialSize) {
        outBuffer.shrink_to_fit();
      }
    }
  }

  size_t outReadableBytes() const {
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    return outBuffer.size() - outReadPos;
  }

  const char *outPeek() const {
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    return outBuffer.data() + outReadPos;
  }

  // 批量操作优化：一次性处理多个数据块
  template <typename Iterator>
  void appendOutBatch(Iterator begin, Iterator end) {
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    size_t totalSize = 0;
    for (auto it = begin; it != end; ++it) {
      totalSize += it->size();
    }

    outBuffer.reserve(outBuffer.size() + totalSize);
    for (auto it = begin; it != end; ++it) {
      outBuffer.insert(outBuffer.end(), it->begin(), it->end());
    }
  }
};

class TcpServer {
public:
  TcpServer(int port, int threadNum);
  ~TcpServer();

  void start();
  void stop();

  static constexpr int MAX_EVENTS = 1024;
  static constexpr int BUFFER_SIZE = 4096;

private:
  int listenFd;
  int _port;

  int epollFd;

  int _threadNum;
  ThreadPool threadPool;

  db database;

  std::atomic<bool> stopFlag;

  std::unordered_map<int, std::shared_ptr<ClientBuffer>> clientBuffers;
  std::mutex clientBuffersMutex;

  void setNonBlocking(int fd);
  void setReuseAddr(int fd);
  void setSocketOptions(int fd);

  void handleEvents();
  void HandleConnection();
  void HandleClient(int clientFd);
  void processRequest(int clientFd, std::string request);
  void sendResp(int clientFd);
};

} // namespace yanhon
