#pragma once
#include <string>

extern "C" {
    __declspec(dllexport) const char* dm_plugin_name();
    __declspec(dllexport) const char* dm_plugin_version();
    __declspec(dllexport) const char* dm_plugin_handle(
        const char* method, const char* args);
    __declspec(dllexport) void dm_plugin_free(const char* s);
}

// 函数指针类型
using NameFn    = const char* (*)();
using VersionFn = const char* (*)();
using HandleFn  = const char* (*)(const char*, const char*);
using FreeFn    = void (*)(const char*);