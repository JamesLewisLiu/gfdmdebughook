#pragma once
#include <windows.h>
#include <atomic>
#include <string>
#include "parameters.h"
namespace debughook {
extern std::atomic_bool active, visible, stopped;
extern Switches switches;
extern Layout layout;
extern std::atomic<unsigned> reads[max_parameter_count];
extern std::atomic<unsigned> rendered;
extern std::atomic_bool panel_ready;
extern HMODULE self;
extern std::wstring directory;
extern std::string profile;
void log(const char* text);
bool start_panel();
void stop_panel();
void change(int index);
bool set_switch(int index, bool enabled);
bool restore_switch(int index);
void all_off();
void restore_native();
}
