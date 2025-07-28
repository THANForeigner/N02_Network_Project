#pragma once
#include<iostream>
#include<Windows.h>
#include <powrprof.h>
#ifndef SHTDN_REASON_MAJOR_OTHER
#define SHTDN_REASON_MAJOR_OTHER 0x00000000L
#endif
bool EnableShutdownPrivilege();
void ShutDown();
void Restart();
void Sleep();