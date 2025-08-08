#include "client.h"
#include "UI.h"
#include "sstream"
#include <thread>   
#include <chrono>
int main()
{
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
      std::this_thread::sleep_for(5s);
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
    std::this_thread::sleep_for(5s);
    Clear();
    client.command.clear();
  }

  client.Shutdown();
  return 0;
}
