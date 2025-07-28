#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

const char* PORT = "27015";
const int   BUFSIZE = 512;

void RunCommand(std::string command)
{

}

int main()
{
    WSADATA wsaData;
    SOCKET listenSock=INVALID_SOCKET;
    SOCKET clientSock=INVALID_SOCKET;
    char command[BUFSIZ];

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
    
    struct addrinfo hints{}, *res=nullptr;

    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;
    hints.ai_flags=AI_PASSIVE;

    if(getaddrinfo(nullptr,PORT,&hints,&res)!=0)
    {
        std::cerr<<"Get address info failed\n";
        WSACleanup();
        return 1;
    }

    listenSock=socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(listenSock==INVALID_SOCKET)
    {
        std::cerr<<"Create listen socket failed\n";
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }
    if(bind(listenSock,res->ai_addr,(int)res->ai_addrlen)==SOCKET_ERROR)
    {
        std::cerr<<"Bind failed\n";
        freeaddrinfo(res);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(res);

    if(listen(listenSock, SOMAXCONN)==SOCKET_ERROR)
    {
        std::cerr<<"Listen failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    clientSock=accept(listenSock,nullptr, nullptr);
    if(clientSock==INVALID_SOCKET)
    {
        std::cerr<<"Accept failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    while(true)
    {
        int lenght = recv(clientSock, command, BUFSIZE, 0);
        if(lenght>0)
        {
            std::string _command=std::string(command,lenght);
            RunCommand(_command);
        }
    }
    shutdown(clientSock,SD_SEND);
    closesocket(clientSock);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}