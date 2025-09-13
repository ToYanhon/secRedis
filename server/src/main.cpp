#include "TcpServer/TcpServer.hpp"
#include <signal.h>

yanhon::TcpServer *globalServer = nullptr;

void handleSignal(int sig) {
  // 忽略 SIGPIPE 信号，防止写操作在对端关闭时终止进程
  if (sig == SIGINT) {
    if (globalServer) {
      globalServer->stop();
      delete globalServer;
      globalServer = nullptr;
    }
    exit(0);
  }
}

int main() {
  globalServer = new yanhon::TcpServer(6379, 4);
  globalServer->start();
  signal(SIGINT, handleSignal);
  return 0;
}