#include "server.h"

void Server::Init()
{
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        exit(1);
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(nullptr, PORT, &hints, &res) != 0)
    {
        std::cerr << "Get address info failed\n";
        WSACleanup();
        exit(1);
    }

    listenSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenSock == INVALID_SOCKET)
    {
        std::cerr << "Create listen socket failed\n";
        freeaddrinfo(res);
        WSACleanup();
        exit(1);
    }
    if (bind(listenSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed\n";
        freeaddrinfo(res);
        closesocket(listenSock);
        WSACleanup();
        exit(1);
    }
    freeaddrinfo(res);

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed\n";
        closesocket(listenSock);
        WSACleanup();
        exit(1);
    }

    clientSock = accept(listenSock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET)
    {
        std::cerr << "Accept failed\n";
        closesocket(listenSock);
        WSACleanup();
        exit(1);
    }
}

void Server::GetCommandFromClient()
{
    int lenght = recv(clientSock, command, BUFSIZE, 0);
    if (lenght > 0)
    {
        ProcessCommand();
    }
}

void Server::Shutdown()
{
    shutdown(clientSock,SD_SEND);
    closesocket(clientSock);
    closesocket(listenSock);
    WSACleanup();
}