#include "plugin_sdk/plugin_api.h"
#include <cstring>
#include <string>

extern "C" {

__declspec(dllexport) const char* dm_plugin_name() {
    return "example";
}

__declspec(dllexport) const char* dm_plugin_version() {
    return "1.0.0";
}

__declspec(dllexport) const char* dm_plugin_handle(
    const char* method, const char* args) {
    std::string result;
    if (std::strcmp(method, "greet") == 0) {
        result = "hello from example plugin, args=" + std::string(args);
    } else if (std::strcmp(method, "version") == 0) {
        result = "example plugin v1.0.0";
    } else if (std::strcmp(method, "echo") == 0) {
        result = std::string(args);
    } else {
        result = "unknown method: " + std::string(method);
    }
    char* buf = new char[result.size() + 1];
    std::strcpy(buf, result.c_str());
    return buf;
}

__declspec(dllexport) void dm_plugin_free(const char* s) {
    delete[] s;
}

} // extern "C"