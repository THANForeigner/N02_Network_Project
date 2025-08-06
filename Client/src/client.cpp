#include "client.h"
#include "fstream"
#include "sstream"
namespace fs = std::filesystem;

void Client::Init(std::string hostIP, std::string _port) {
  if (hostIP.empty())
    serverHost = "localhost";
  else
    serverHost = hostIP;
  if (port.empty()) {
    port = PORT;
  } else
    port = _port;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed\n";
    exit(1);
  }
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  int rc = getaddrinfo(serverHost.c_str(), port.c_str(), &hints, &res);
  if (rc != 0) {
    std::cerr << "Get address info failed (" << rc << ")\n";
    WSACleanup();
    exit(1);
  }

  connSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (connSock == INVALID_SOCKET) {
    std::cerr << "Create socket failed\n";
    freeaddrinfo(res);
    WSACleanup();
    exit(1);
  }

  if (connect(connSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
    std::cerr << "Unable to connect to server\n";
    closesocket(connSock);
    freeaddrinfo(res);
    WSACleanup();
    exit(1);
  }
}

void Client::GetCommand() {
  std::ifstream fs("../client_secret.json");
  if (fs) {
    GmailClient gmail;
    gmail.GetLatestEmailBody(command);
  } else {
    std::cout << "Enter Command: \n";
    std::getline(std::cin >> std::ws, this->command);
  }
  fs.close();
}

void Client::SendCommand() {
  if (!send(connSock, command.c_str(), command.size(), 0)) {
    std::cerr << "Send failed\n";
  }
}

void Client::ReceiveFile(std::string pathFolder) {
  auto read_line_from_socket = [this]() -> std::string {
    std::string line;
    char c;
    while (recv(this->connSock, &c, 1, 0) > 0) {
      if (c == '\n') {
        break;
      }
      line += c;
    }
    return line;
  };

  std::string path_header = read_line_from_socket();
  if (path_header.rfind("ERROR:", 0) == 0) {
    std::cerr << "Server reported an error: " << path_header.substr(6)
              << std::endl;
    return;
  }

  std::string size_header = read_line_from_socket();
  if (path_header.rfind("PATH:", 0) != 0 ||
      size_header.rfind("SIZE:", 0) != 0) {
    std::cerr << "Protocol error: Received invalid headers from server."
              << std::endl;
    return;
  }
  std::string relative_path_str = path_header.substr(5);
  long long file_size = 0;
  try {
    file_size = std::stoll(size_header.substr(5));
  } catch (const std::invalid_argument &e) {
    std::cerr << "Protocol error: Invalid file size received." << std::endl;
    return;
  }

  if (file_size == 0) {
    std::cout << "Server sent an empty file. Nothing to save." << std::endl;
    return;
  }
  std::string server_path_str = path_header.substr(5);
  fs::path received_path(server_path_str);
  std::string filename = received_path.filename().string();
  fs::path final_destination_path = fs::path(pathFolder) / filename;
  fs::path destination_dir = final_destination_path.parent_path();
  try {
    if (!fs::exists(destination_dir)) {
      fs::create_directories(destination_dir);
      std::cout << "Created directory: " << destination_dir.string()
                << std::endl;
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error creating directories: " << e.what() << std::endl;
    return;
  }
  std::ofstream outfile(final_destination_path, std::ios::binary);
  if (!outfile.is_open()) {
    std::cerr << "Failed to open file for writing: " << final_destination_path
              << std::endl;
    return;
  }

  std::cout << "Receiving file to: " << final_destination_path << " ("
            << file_size << " bytes)" << std::endl;

  std::vector<char> buffer(8192);
  long long total_bytes_received = 0;
  int bytes_read = 0;

  while (total_bytes_received < file_size) {
    bytes_read = recv(connSock, buffer.data(), buffer.size(), 0);

    if (bytes_read > 0) {
      outfile.write(buffer.data(), bytes_read);
      total_bytes_received += bytes_read;
    } else if (bytes_read == 0) {
      std::cerr << "Connection closed by server prematurely." << std::endl;
      break;
    } else {
      std::cerr << "Socket error during transfer: " << WSAGetLastError()
                << std::endl;
      break;
    }
  }

  outfile.close();

  if (total_bytes_received == file_size) {
    std::cout << "File transfer completed successfully." << std::endl;
  } else {
    std::cerr << "File transfer incomplete. Received " << total_bytes_received
              << " of " << file_size << " bytes." << std::endl;
  }
}

// void Client::sendFile(std::string pathFile){
//    GmailClient gmail;
//    gmail.SendEmaiiAttachment("");
// }
// void Client::Shutdown()
{
  shutdown(connSock, SD_SEND);
  closesocket(connSock);
  WSACleanup();
}
