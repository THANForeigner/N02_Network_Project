#include "client.h"
#include "UI.h"
#include "sstream"
#include <thread>   
#include <chrono>
int main()
{
  using namespace std::this_thread;
  using namespace std::chrono;
  Client client;
  std::string ip, port;
  DrawGetIPPORT(ip, port);
  while (true)
  {
    if(!client.Init(ip, port))
    {
      sleep_for(nanoseconds(10));
      sleep_until(system_clock::now() + seconds(30));
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
    if(client.fromEmail)
    {
      
    }
    client.command.clear();
  }

  client.Shutdown();
  return 0;
}
