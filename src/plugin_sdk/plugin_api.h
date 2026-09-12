#pragma once
#include <string>

extern "C" {
    __declspec(dllexport) const char* dm_plugin_name();
    __declspec(dllexport) const char* dm_plugin_version();
    __declspec(dllexport) const char* dm_plugin_handle(
        const char* method, const char* args);
    __declspec(dllexport) void dm_plugin_free(const char* s);
}
