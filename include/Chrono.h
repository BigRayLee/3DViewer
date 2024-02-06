#pragma once
#include <chrono>

void TimerStart();
unsigned int TimerStop(const char *str = "");
char* GetCurrentTime();
void GetElapsedTime(timeval start, timeval end, const char* str);

