#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

const char *PORT = "27015";
const int BUFSIZE = 512;

int main()
{
    std::string serverHost;
    std::cout << "Enter server IP or hostname (blank for localhost): ";
    std::getline(std::cin, serverHost);
    if (serverHost.empty())
        serverHost = "localhost";

    WSADATA wsaData;
    SOCKET connSock = INVALID_SOCKET;
    struct addrinfo hints{}, *res = nullptr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(serverHost.c_str(), PORT, &hints, &res);
    if (rc != 0)
    {
        std::cerr << "Get address info failed (" << rc << ")\n";
        WSACleanup();
        return 1;
    }

    connSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connSock == INVALID_SOCKET)
    {
        std::cerr << "Create socket failed\n";
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    if (connect(connSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
    {
        std::cerr << "Unable to connect to server\n";
        closesocket(connSock);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    char command[BUFSIZE];
    while (true)
    {
        // getcommand
    }
    shutdown(connSock, SD_BOTH);
    closesocket(connSock);
    return 0;
}