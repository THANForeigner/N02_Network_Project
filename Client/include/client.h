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
          TAKE_SCREEN_SHOT,
          SLEEP,
          RESTART,
          SHUTDOWN
      };
  */
  Client() {};
  std::string port; //Port kết nối với Server
  std::string serverHost; //Socket giao tiếp với Server
  WSADATA wsaData; 
  SOCKET connSock = INVALID_SOCKET; //Socket giao tiếp với Server 
  struct addrinfo hints{}, *res = nullptr;
  // hints sử cung để lưu thông tin Socket 
  // res dùng để trỏ tới dang sách kết quả địa chỉ sau khi gọi hàm getaddrinfo() 
  std::string command; //Lệnh gửi qua Server
  bool fromEmail=0; //Flag kiểm tra lệnh nhận từ mail hay console
  std::string receiver; //Người nhận mail
  std::string filename; //Tên file

  const char *PORT = "27015"; //Port mặc định
  const int BUFSIZE = 512; //Size tối đa của command mặc định

  bool Init(std::string hostIP, std::string port);
  void GetCommand();
  void SendCommand();
  void sendFile(std::string pathFile);
  void ProcessCommand();
  void ReceiveFile(std::string pathFolder);
  void Shutdown();
};
