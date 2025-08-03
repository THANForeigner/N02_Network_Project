#include "client.h"
#include "sstream"
#include "fstream"
namespace fs = std::filesystem;

void Client::Init(std::string hostIP, std::string _port)
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
        port = _port;
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
    std::ifstream fs("client_secret.json");
    if (fs)
    {
        GmailClient gmail;
        gmail.GetLatestEmailBody(command);
    }
    else 
    {
        std::cout << "Enter Command: \n";
        std::getline(std::cin >> std::ws, this->command);
    }
    fs.close();
}

void Client::SendCommand()
{
    if (!send(connSock, command.c_str(), command.size(), 0))
    {
        std::cerr << "Send failed\n";
    }
}

void Client::ReceiveFile(std::string pathFolder)
{
    // --- 1. Read Headers from the Server ---

    // Helper lambda to read one line at a time from the socket.
    // This is robust against TCP packet fragmentation.
    auto read_line_from_socket = [this]() -> std::string
    {
        std::string line;
        char c;
        // Read one character at a time until a newline is found.
        while (recv(this->connSock, &c, 1, 0) > 0)
        {
            if (c == '\n')
            {
                break;
            }
            line += c;
        }
        return line;
    };

    std::string path_header = read_line_from_socket();

    // --- 2. Parse Headers and Handle Errors ---

    // Check for an initial error message from the server.
    if (path_header.rfind("ERROR:", 0) == 0)
    {
        std::cerr << "Server reported an error: " << path_header.substr(6) << std::endl;
        return;
    }

    std::string size_header = read_line_from_socket();

    // Validate that the headers match the expected protocol.
    if (path_header.rfind("PATH:", 0) != 0 || size_header.rfind("SIZE:", 0) != 0)
    {
        std::cerr << "Protocol error: Received invalid headers from server." << std::endl;
        return;
    }

    // Extract the relative path and file size.
    std::string relative_path_str = path_header.substr(5); // Skip "PATH:"
    long long file_size = 0;
    try
    {
        file_size = std::stoll(size_header.substr(5)); // Skip "SIZE:"
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Protocol error: Invalid file size received." << std::endl;
        return;
    }

    if (file_size == 0)
    {
        std::cout << "Server sent an empty file. Nothing to save." << std::endl;
        return;
    }

    // --- 3. Construct Destination Path and Create Directories ---

    // Combine the user-provided base folder with the relative path from the server.
    fs::path final_destination_path = fs::path(pathFolder) / relative_path_str;
    fs::path destination_dir = final_destination_path.parent_path();

    // Ensure the destination directory exists, creating it if necessary.
    try
    {
        if (!fs::exists(destination_dir))
        {
            fs::create_directories(destination_dir);
            std::cout << "Created directory: " << destination_dir.string() << std::endl;
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        return;
    }

    // --- 4. Open Local File and Receive Data ---

    // Open the file for writing in binary mode. This is critical.
    std::ofstream outfile(final_destination_path, std::ios::binary);
    if (!outfile.is_open())
    {
        std::cerr << "Failed to open file for writing: " << final_destination_path << std::endl;
        return;
    }

    std::cout << "Receiving file to: " << final_destination_path << " (" << file_size << " bytes)" << std::endl;

    // Read from the socket and write to the file in chunks.
    std::vector<char> buffer(8192); // 8KB buffer
    long long total_bytes_received = 0;
    int bytes_read = 0;

    while (total_bytes_received < file_size)
    {
        bytes_read = recv(connSock, buffer.data(), buffer.size(), 0);

        if (bytes_read > 0)
        {
            outfile.write(buffer.data(), bytes_read);
            total_bytes_received += bytes_read;
        }
        else if (bytes_read == 0)
        {
            std::cerr << "Connection closed by server prematurely." << std::endl;
            break;
        }
        else
        { // bytes_read < 0
            std::cerr << "Socket error during transfer: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    outfile.close();

    // --- 5. Final Verification ---
    if (total_bytes_received == file_size)
    {
        std::cout << "File transfer completed successfully." << std::endl;
    }
    else
    {
        std::cerr << "File transfer incomplete. Received " << total_bytes_received
                  << " of " << file_size << " bytes." << std::endl;
    }
}

void Client::Shutdown()
{
    shutdown(connSock, SD_SEND);
    closesocket(connSock);
    WSACleanup();
}
