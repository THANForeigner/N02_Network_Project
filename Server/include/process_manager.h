#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <string>

bool startProcess(const std::wstring& appName);
bool stopProcess(const std::wstring& targetProcessName);
void listProcesses();

#endif // PROCESS_MANAGER_H
