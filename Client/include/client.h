#pragma once
#define WIN32_LEAN_AND_MEAN
#include "gmail.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

class Client {
public:
  /*    enum Command{
          COPYFILE, //COPYFILE path
          TOGGLE_VIDEO,
          GET_VIDEO,
          TOGGLE_KEYLOGGER,
          GET_KEYLOGGER,
          GET_RUNNING_PROCESS,
          RUN_PROCESS, //RUN_PROCESS name
          SHUTDOWN_PROCESS, //SHUTDOWN_PROCESS name
          SLEEP,
          RESTART,
          SHUTDOWN
      };
  */
  Client() {};
  std::string port;
  std::string serverHost;
  WSADATA wsaData;
  SOCKET connSock = INVALID_SOCKET;
  struct addrinfo hints{}, *res = nullptr;

  std::string command;
  bool fromEmail=0;
  std::string receiver;
  std::string filename;

  const char *PORT = "27015";
  const int BUFSIZE = 512;

  bool Init(std::string hostIP, std::string port);
  void GetCommand();
  void SendCommand();
  void sendFile(std::string pathFile);
  void ProcessCommand();
  void ReceiveFile(std::string pathFolder);
  void Shutdown();
};
