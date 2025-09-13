#include <TcpClient/TcpClient.hpp>

// 简单的命令行界面
int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <host> <port>" << std::endl;
    return 1;
  }

  std::string host = argv[1];
  int port = std::stoi(argv[2]);

  TcpClient client(host, port);

  std::cout << "Redis-like Client. Type 'quit' to exit." << std::endl;

  while (true) {
    std::cout << "> ";
    std::string input;
    std::getline(std::cin, input);

    if (input == "quit") {
      break;
    }

    if (input.empty()) {
      continue;
    }

    std::string response = client.sendCommand(input);
    std::cout << response << std::endl;
  }

  return 0;
}