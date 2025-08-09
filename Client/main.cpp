#include "UI.h"
#include "client.h"
#include "include/gmail.h"
#include "sstream"
#include <chrono>
#include <gmail.h>
#include <iostream>
#include <thread>
int main() {
  using namespace std::chrono_literals;
  using namespace std::this_thread;
  using namespace std::chrono;
  Client client;
  std::string ip, port;
  DrawGetIPPORT(ip, port);
  while (true)
  {
    if(!client.Init(ip, port))
    {
      std::this_thread::sleep_for(3s);
    }
    else break;
    Clear();
    DrawGetIPPORT(ip, port);
  }
  while (true)
  {
    DrawMenu();
    client.GetCommand();
    if (client.command == "EXIT")
      break;
    client.ProcessCommand();
    std::this_thread::sleep_for(3s);
    //Clear();
    client.command.clear();
  }
  
  client.Shutdown();
  return 0;
}
