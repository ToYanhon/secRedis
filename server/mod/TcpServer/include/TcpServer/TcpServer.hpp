#pragma once
#include <atomic>

#include "MutexGuard/MutexGuard.hpp"
#include "ThreadPool/ThreadPool.hpp"
#include "db/db.hpp"
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace yanhon {

struct ClientBuffer {
  std::vector<char> data;
  size_t readOffset = 0;     // 已解析到的位置
  size_t writeOffset = 0;    // 已写入数据的位置
  size_t expectedLength = 0; // 期望的消息长度

  std::vector<char> outBuffer; // 待发送缓冲区
  size_t outOffset = 0;        // outBuffer 当前发送位置

  static constexpr size_t kInitialSize = 4096;

  mutable pthread_mutex_t mutex;

  ClientBuffer(size_t initialSize = kInitialSize) {
    pthread_mutex_init(&mutex, nullptr);
    data.resize(initialSize);
  }

  ~ClientBuffer() { pthread_mutex_destroy(&mutex); }

  // 确保 data 可写空间
  void ensureWritable(size_t n) {
    if (writeOffset + n > data.size()) {
      // 如果前面有空闲空间，尝试回收
      if (readOffset > 0) {
        size_t readable = readableBytes();
        memmove(data.data(), data.data() + readOffset, readable);
        writeOffset = readable;
        readOffset = 0;
      }
      // 如果仍然不足，扩容
      if (writeOffset + n > data.size()) {
        data.resize(writeOffset + n);
      }
    }
  }

  // 可读字节数
  size_t readableBytes() const {
    MutexGuard lock(mutex);
    return writeOffset - readOffset;
  }

  // 当前可读数据指针
  const char *peek() const {
    MutexGuard lock(mutex);
    return data.data() + readOffset;
  }

  // 消费 n 字节
  void retrieve(size_t n) {
    MutexGuard lock(mutex);
    readOffset += n;
    if (readOffset == writeOffset) {
      // 数据完全消费，重置偏移并可释放内存
      readOffset = writeOffset = 0;
      if (data.capacity() > 4 * kInitialSize) {
        data.shrink_to_fit();
        data.resize(kInitialSize);
      }
    }
  }

  // 写入数据
  void append(const char *src, size_t n) {
    MutexGuard lock(mutex);
    ensureWritable(n);
    std::copy(src, src + n, data.begin() + writeOffset);
    writeOffset += n;
  }

  // 向 outBuffer 写入数据
  void appendOut(const char *src, size_t n) {
    MutexGuard lock(mutex);
    size_t oldSize = outBuffer.size();
    outBuffer.resize(oldSize + n);
    memcpy(outBuffer.data() + oldSize, src, n);
  }

  // 发送完部分 outBuffer 后回收已发送的空间
  void retrieveOut(size_t n) {
    MutexGuard lock(mutex);
    outOffset += n;
    if (outOffset >= outBuffer.size()) {
      // 所有数据发送完毕
      outBuffer.clear();
      outOffset = 0;
    } else if (outOffset > outBuffer.size() / 2) {
      // 已发送数据占据一半以上，移动未发送部分，释放前面空间
      size_t remaining = outBuffer.size() - outOffset;
      memmove(outBuffer.data(), outBuffer.data() + outOffset, remaining);
      outBuffer.resize(remaining);
      outOffset = 0;
    }
  }

  // 当前 outBuffer 未发送长度
  size_t outReadableBytes() const {
    MutexGuard mg(mutex);
    return outBuffer.size() - outOffset;
  }

  // 指向当前未发送位置
  const char *outPeek() const {
    MutexGuard mg(mutex);
    return outBuffer.data() + outOffset;
  }
};

class TcpServer {
public:
  TcpServer(int port, int threadNum);
  ~TcpServer();

  void start();
  void stop();

  static const int MAX_EVENTS = 1024;
  static const int BUFFER_SIZE = 4096;

private:
  int listenFd;
  int _port;

  int epollFd;

  int _threadNum;
  ThreadPool threadPool;

  db database;

  std::atomic<bool> stopFlag;

  std::unordered_map<int, std::shared_ptr<ClientBuffer>> clientBuffers;
  pthread_mutex_t clientBuffersMutex;

  void setNonBlocking(int fd);
  void serReuseAddr(int fd);

  void HandleConnection();
  void HandleClient(int clientFd);

  void doResponse(int clientFd, std::string request);
  void HandleResp(int clientFd);
};

} // namespace yanhon
