#include "PowerRelatedFunction.h"

bool EnableShutdownPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        std::cerr << "OpenProcessToken failed: " << GetLastError() << "\n";
        CloseHandle(hToken);
        return false;
    }
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid))
    {
        std::cerr << "LookupPrivilegeValue failed: " << GetLastError() << '\n';
        CloseHandle(hToken);
        return false;
    }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL))
    {
        std::cerr << "AdjustTokenPrivileges failed: " << GetLastError() << "\n";
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);
    return true;
}

void ShutDown()
{
    if (EnableShutdownPrivilege())
    {
        if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER))
        {
            std::cerr << "ExitWindowsEx failed: " << GetLastError() << "\n";
        }
    }
    else
    {
        std::cerr << "Failed to enable shutdown privilege.\n";
    }
}

void Restart()
{
    if (EnableShutdownPrivilege())
    {
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER))
        {
            std::cerr << "ExitWindowsEx failed: " << GetLastError() << "\n";
        }
    }
    else
    {
        std::cerr << "Failed to enable shutdown privilege.\n";
    }
}

void Sleep()
{
    BOOL result = SetSuspendState(FALSE, TRUE, FALSE);
    if (!result) {
        std::cerr << "SetSuspendState failed: " << GetLastError() << "\n";
        std::cerr<<"Failed to enter sleep mode\n"<<'\n';
    }
}
