#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <string>
#include <iostream>
#include "process_manager.h"

bool startProcess(const std::wstring& appName) {
    HINSTANCE result = ShellExecuteW(
        NULL, L"open", appName.c_str(), NULL, NULL, SW_SHOW
    );

    if ((INT_PTR)result > 32) {
        std::wcout << L"Successfully started: " << appName << std::endl;
        return true;
    }
    else {
        std::wcerr << L"Failed to start: " << appName
                   << L"\nError code: " << (INT_PTR)result << std::endl;
        return false;
    }
}

bool stopProcess(const std::wstring& targetProcessName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to take process snapshot.\n";
        return false;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, targetProcessName.c_str()) == 0) {
                // Tìm đúng tiến trình
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 0)) {
                        std::wcout << L"Terminated process: " << targetProcessName << std::endl;
                        found = true;
                    }
                    else {
                        std::wcerr << L"Failed to terminate: " << targetProcessName
                                   << L" (PID: " << pe.th32ProcessID << L")\n";
                    }
                    CloseHandle(hProcess);
                }
                else {
                    std::wcerr << L"Could not open process: " << pe.th32ProcessID << std::endl;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

void listProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to take process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        std::wcout << L"--- Running Processes ---\n";
        do {
            std::wcout << L"PID: " << pe.th32ProcessID
                       << L" | Name: " << pe.szExeFile << std::endl;
        } while (Process32NextW(hSnapshot, &pe));
    } else {
        std::wcerr << L"Failed to enumerate processes." << std::endl;
    }

    CloseHandle(hSnapshot);
}
