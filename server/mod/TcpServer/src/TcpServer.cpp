#include "TcpServer/TcpServer.hpp"

#include "Logger/Logger.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iterator>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>

namespace yanhon {
TcpServer::TcpServer(int port, int threadNum)
    : listenFd(-1), _port(port), epollFd(-1), _threadNum(threadNum),
      threadPool(threadNum), stopFlag(false) {
  // Initialize server (e.g., create socket, bind, listen)
  ADD_CONSOLE_LOGGER();
  ADD_FILE_LOGGER("server.log");

  pthread_mutex_init(&clientBuffersMutex, nullptr);

  LOG_INFO("TcpServer initialized on port " + std::to_string(port) + " with " +
           std::to_string(threadNum) + " threads.");
}

TcpServer::~TcpServer() { stop(); }

void TcpServer::start() {
  if (stopFlag.load()) {
    LOG_WARNING("TcpServer is already running.");
    return;
  }
  listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd < 0) {
    LOG_ERROR("Failed to create socket." + std::string(strerror(errno)));
    return;
  }

  setNonBlocking(listenFd);
  serReuseAddr(listenFd);

  if (_port <= 0 || _port > 65535) {
    LOG_ERROR("Invalid port number: " + std::to_string(_port));
    close(listenFd);
    return;
  }

  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(_port);

  if (bind(listenFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    LOG_ERROR("Bind failed on port " + std::to_string(_port) + ": " +
              std::string(strerror(errno)));
    return;
  }

  if (listen(listenFd, SOMAXCONN) < 0) {
    LOG_ERROR("Listen failed on port " + std::to_string(_port) + ": " +
              std::string(strerror(errno)));
    return;
  }

  LOG_INFO("TcpServer started and listening on port " + std::to_string(_port));

  epollFd = epoll_create1(0);
  if (epollFd < 0) {
    LOG_ERROR("Failed to create epoll instance." +
              std::string(strerror(errno)));
    return;
  }

  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = listenFd;
  if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &event) < 0) {
    LOG_ERROR("Failed to add listenFd to epoll." +
              std::string(strerror(errno)));
    return;
  }

  while (!stopFlag.load()) {
    struct epoll_event events[MAX_EVENTS];
    int n = epoll_wait(epollFd, events, MAX_EVENTS, -1);
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == listenFd) {
        // New incoming connection
        threadPool.enqueue(std::bind(&TcpServer::HandleConnection, this));
      } else if (events[i].events & EPOLLIN) {
        // Handle data from connected clients
        int clientFd = events[i].data.fd;

        threadPool.enqueue(std::bind(&TcpServer::HandleClient, this, clientFd));
      } else if (events[i].events & EPOLLOUT) {
        int fd = events[i].data.fd;
        threadPool.enqueue(std::bind(&TcpServer::HandleResp, this, fd));
      } else {
        // Other events (e.g., error)
        int clientFd = events[i].data.fd;
        LOG_ERROR("Epoll error on fd " + std::to_string(clientFd));
        close(clientFd);
        {
          MutexGuard lock(clientBuffersMutex);
          clientBuffers.erase(clientFd);
        }
      }
    }
  }
}

void TcpServer::stop() {
  if (stopFlag.load()) {
    LOG_WARNING("TcpServer is already stopped.");
    return;
  }
  stopFlag.store(true);
  close(epollFd);
  threadPool.stop();
  close(listenFd);
  LOG_INFO("TcpServer on port " + std::to_string(_port) + " has been stopped.");
  pthread_mutex_destroy(&clientBuffersMutex);
}

void TcpServer::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  auto rt = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (rt == -1) {
    LOG_ERROR("Failed to set non-blocking mode." +
              std::string(strerror(errno)));
  }
}

void TcpServer::serReuseAddr(int fd) {
  int opt = 1;
  auto rt = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (rt == -1) {
    LOG_ERROR("Failed to set SO_REUSEADDR." + std::string(strerror(errno)));
  }
}

void TcpServer::HandleConnection() {
  while (true) {
    // Accept new connection
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    int connFd = accept(listenFd, (struct sockaddr *)&clientAddr, &clientLen);
    if (connFd >= 0) {
      setNonBlocking(connFd);
      struct epoll_event connEvent;
      connEvent.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
      connEvent.data.fd = connFd;

      if (epoll_ctl(epollFd, EPOLL_CTL_ADD, connFd, &connEvent) < 0) {
        LOG_ERROR("Failed to add connFd to epoll." +
                  std::string(strerror(errno)));
        close(connFd);
      } else {
        LOG_INFO("Accepted new connection.");
      }
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No more connections to accept
        break;
      } else {
        LOG_ERROR("Error accepting connection." + std::string(strerror(errno)));
        break;
      }
    }
  }
}

// 4 bytes length prefix + payload
void TcpServer::HandleClient(int clientFd) {
  // Handle client data
  char buffer[BUFFER_SIZE];

  std::shared_ptr<ClientBuffer> clientBuffer;
  {
    MutexGuard lock(clientBuffersMutex);

    auto [it, _] =
        clientBuffers.emplace(clientFd, std::make_shared<ClientBuffer>());
    clientBuffer = it->second;
  }

  size_t &expectedLength = clientBuffer->expectedLength;

  while (true) {
    ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
      // Process data
      clientBuffer->append(buffer, bytesRead);
      while (true) {
        if (expectedLength == 0) {
          if (clientBuffer->readableBytes() < 4) {
            break;
          }
          uint32_t netlen = 0;
          std::memcpy(&netlen, clientBuffer->peek(), sizeof(netlen));
          expectedLength = static_cast<int32_t>(ntohl(netlen));

          clientBuffer->retrieve(4);
          if (expectedLength > BUFFER_SIZE) {
            LOG_ERROR("Message too large, closing connection.");
            epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
            close(clientFd);
            {
              MutexGuard lock(clientBuffersMutex);
              clientBuffers.erase(clientFd);
            }
            return;
          }
        }
        if (clientBuffer->readableBytes() >= expectedLength) {
          threadPool.enqueue(
              std::bind(&TcpServer::doResponse, this, clientFd,
                        std::string(clientBuffer->peek(), expectedLength)));
          clientBuffer->retrieve(expectedLength);
          expectedLength = 0;
        }
      }

    } else if (bytesRead == 0) {
      // Client disconnected
      auto rt = epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
      if (rt == -1) {
        LOG_ERROR("Failed to remove clientFd from epoll." +
                  std::string(strerror(errno)));
      }
      close(clientFd);
      LOG_INFO("Client disconnected.");
      {
        MutexGuard lock(clientBuffersMutex);
        clientBuffers.erase(clientFd);
      }

      break;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        ev.data.fd = clientFd;
        auto rt = epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &ev);
        if (rt == -1) {
          LOG_ERROR("Failed to modify clientFd in epoll." +
                    std::string(strerror(errno)));
        }
        // No more data to read
        break;
      } else {
        // Error occurred
        LOG_ERROR("Error reading from client.");
        auto rt = epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
        if (rt == -1) {
          LOG_ERROR("fail del" + std::string(strerror(errno)));
        }
        close(clientFd);
        {
          MutexGuard lock(clientBuffersMutex);
          clientBuffers.erase(clientFd);
        }
        break;
      }
    }
  }
}

// set,del,get,exists 命令
void TcpServer::doResponse(int clientFd, std::string request) {
  std::istringstream iss(request);
  std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                  std::istream_iterator<std::string>{}};
  std::ostringstream response;

  if (tokens.empty()) {
    response << "ERR empty command";
  } else {
    std::string cmd = tokens[0];
    for (auto &c : cmd)
      c = std::toupper(c); // 忽略大小写

    if (cmd == "SET") {
      if (tokens.size() < 3) {
        response << "ERR wrong number of arguments for 'SET'";
      } else {
        const std::string &key = tokens[1];

        // 检查是否有类型指示符
        if (tokens[2] == "MAP") {
          // 处理 map 类型
          if (tokens.size() < 4 || tokens.size() % 2 != 0) {
            response << "ERR wrong number of arguments for map";
          } else {
            std::unordered_map<std::string, std::string> map;
            for (size_t i = 3; i < tokens.size(); i += 2) {
              map[tokens[i]] = tokens[i + 1];
            }
            database.set(key, map);
            response << "OK";
            LOG_INFO("SET map key: " + key + " with " +
                     std::to_string(map.size()) + " elements");
          }
        } else if (tokens[2] == "SET") {
          // 处理 set 类型
          if (tokens.size() < 4) {
            response << "ERR wrong number of arguments for set";
          } else {
            std::unordered_set<std::string> set;
            for (size_t i = 3; i < tokens.size(); i++) {
              set.insert(tokens[i]);
            }
            database.set(key, set);
            response << "OK";
            LOG_INFO("SET set key: " + key + " with " +
                     std::to_string(set.size()) + " elements");
          }
        } else {
          // 默认处理为字符串类型
          // 将所有剩余 token 连接起来作为值
          std::string value;
          for (size_t i = 2; i < tokens.size(); i++) {
            if (i > 2)
              value += " ";
            value += tokens[i];
          }
          database.set(key, value);
          response << "OK";
          LOG_INFO("SET string key: " + key + ", value: " + value);
        }
      }
    } else if (cmd == "GET") {
      if (tokens.size() != 2) {
        response << "ERR wrong number of arguments for 'GET'";
      } else {
        const std::string &key = tokens[1];
        LOG_INFO("GET command received for key: " + key);
        auto val = database.get(key);
        if (!val.has_value()) {
          response << "(nil)";
          LOG_INFO("Key not found: " + key);
        } else {
          if (std::holds_alternative<std::string>(*val)) {
            std::string value = std::get<std::string>(*val);
            response << value;
            LOG_INFO("GET key: " + key + ", value: " + value);
          } else if (std::holds_alternative<
                         std::unordered_map<std::string, std::string>>(*val)) {
            // 处理 map 类型
            auto &map =
                std::get<std::unordered_map<std::string, std::string>>(*val);
            response << "{";
            bool first = true;
            for (const auto &[k, v] : map) {
              if (!first)
                response << ", ";
              response << k << ": " << v;
              first = false;
            }
            response << "}";
            LOG_INFO("GET key: " + key + ", value: (map with " +
                     std::to_string(map.size()) + " elements)");
          } else if (std::holds_alternative<std::unordered_set<std::string>>(
                         *val)) {
            // 处理 set 类型
            auto &set = std::get<std::unordered_set<std::string>>(*val);
            response << "{";
            bool first = true;
            for (const auto &element : set) {
              if (!first)
                response << ", ";
              response << element;
              first = false;
            }
            response << "}";
            LOG_INFO("GET key: " + key + ", value: (set with " +
                     std::to_string(set.size()) + " elements)");
          } else {
            response << "(unknown type)";
            LOG_INFO("GET key: " + key + ", value: (unknown type)");
          }
        }
      }
    } else if (cmd == "DEL") {
      if (tokens.size() != 2) {
        response << "ERR wrong number of arguments for 'DEL'";
      } else {
        const std::string &key = tokens[1];
        bool ok = database.del(key);
        response << (ok ? "(integer) 1" : "(integer) 0");
      }
    } else if (cmd == "EXISTS") {
      if (tokens.size() != 2) {
        response << "ERR wrong number of arguments for 'EXISTS'";
      } else {
        const std::string &key = tokens[1];
        bool ok = database.exists(key);
        response << (ok ? "(integer) 1" : "(integer) 0");
      }
    } else {
      response << "ERR unknown command '" << cmd << "'";
    }
  }

  std::shared_ptr<ClientBuffer> clientBuffer;
  {
    MutexGuard lock(clientBuffersMutex);
    auto it = clientBuffers.find(clientFd);
    if (it == clientBuffers.end()) {
      LOG_ERROR("Client buffer not found for fd " + std::to_string(clientFd));
      return;
    }
    clientBuffer = it->second;
  }

  std::string respStr = response.str();

  // 直接在 outBuffer 尾部写入长度 + 数据
  uint32_t len = htonl(respStr.size());
  clientBuffer->appendOut((char *)&len, sizeof(len));
  clientBuffer->appendOut(respStr.data(), respStr.size());

  // 修改 epoll 事件，监听 EPOLLOUT
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
  ev.data.fd = clientFd;
  epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &ev);
}

void TcpServer::HandleResp(int clientFd) {
  std::shared_ptr<ClientBuffer> clientBuffer;
  {
    MutexGuard lock(clientBuffersMutex);
    auto it = clientBuffers.find(clientFd);
    if (it == clientBuffers.end()) {
      // No buffer found, nothing to send
      return;
    }

    clientBuffer = it->second;
  }

  while (clientBuffer->outReadableBytes()) {
    ssize_t bytesSent = send(clientFd, clientBuffer->outPeek(),
                             clientBuffer->outReadableBytes(), 0);
    if (bytesSent > 0) {
      clientBuffer->retrieveOut(bytesSent);
    } else if (bytesSent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Socket not ready for writing
      break;
    } else {
      // Error occurred
      LOG_ERROR("Error writing to client.");
      epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
      close(clientFd);
      {
        MutexGuard lock(clientBuffersMutex);
        clientBuffers.erase(clientFd);
      }
      return;
    }
  }

  // 如果数据发送完毕，修改事件只监听 EPOLLIN
  if (clientBuffer->outBuffer.empty()) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = clientFd;
    auto rt = epoll_ctl(epollFd, EPOLL_CTL_MOD, clientFd, &ev);
    if (rt == -1) {
      LOG_ERROR("Failed to modify clientFd in epoll.");
    }
  }
}

} // namespace yanhon