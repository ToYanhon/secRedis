#pragma once
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

class TcpClient {
public:
  TcpClient(const std::string &host, int port)
      : host_(host), port_(port), sockfd_(-1) {}

  ~TcpClient() { disconnect(); }

  bool connect() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
      std::cerr << "Failed to create socket" << std::endl;
      return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address/Address not supported" << std::endl;
      return false;
    }

    if (::connect(sockfd_, (struct sockaddr *)&server_addr,
                  sizeof(server_addr)) < 0) {
      std::cerr << "Connection failed" << std::endl;
      return false;
    }

    std::cout << "Connected to server " << host_ << ":" << port_ << std::endl;
    return true;
  }

  void disconnect() {
    if (sockfd_ >= 0) {
      close(sockfd_);
      sockfd_ = -1;
    }
  }

  std::string sendCommand(const std::string &command) {
    if (sockfd_ < 0) {
      if (!connect()) {
        return "Connection failed";
      }
    }

    // 添加长度前缀
    uint32_t length = htonl(command.size());
    std::vector<char> buffer(sizeof(length) + command.size());
    memcpy(buffer.data(), &length, sizeof(length));
    memcpy(buffer.data() + sizeof(length), command.data(), command.size());

    // 发送请求
    if (send(sockfd_, buffer.data(), buffer.size(), 0) < 0) {
      disconnect();
      return "Send failed";
    }

    // 接收响应长度前缀
    uint32_t responseLength;
    if (recv(sockfd_, &responseLength, sizeof(responseLength), MSG_WAITALL) !=
        sizeof(responseLength)) {
      disconnect();
      return "Failed to receive response length";
    }

    responseLength = ntohl(responseLength);

    // 接收响应数据
    std::vector<char> responseData(responseLength);
    if (recv(sockfd_, responseData.data(), responseLength, MSG_WAITALL) !=
        responseLength) {
      disconnect();
      return "Failed to receive response data";
    }

    return std::string(responseData.data(), responseLength);
  }

  std::string set(const std::string &key, const std::string &value) {
    return sendCommand("SET " + key + " " + value);
  }

  std::string get(const std::string &key) { return sendCommand("GET " + key); }

  std::string del(const std::string &key) { return sendCommand("DEL " + key); }

  std::string exists(const std::string &key) {
    return sendCommand("EXISTS " + key);
  }

private:
  std::string host_;
  int port_;
  int sockfd_;
};