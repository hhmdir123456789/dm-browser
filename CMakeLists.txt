cmake_minimum_required(VERSION 3.20)
project(dm_browser C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(DM_PLATFORM_WINDOWS)
endif()

# ============================================================
# sqlite3 amalgamation
# ============================================================
add_library(sqlite3 STATIC
    third_party/sqlite3/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party/sqlite3)
target_compile_options(sqlite3 PRIVATE /w /utf-8)

# ============================================================
# 插件宿主进程
# ============================================================
add_executable(dm_plugin_host
    src/ipc/Pipe.cpp
    src/ipc/Framing.cpp
    src/plugin_host/plugin_host_main.cpp
)
target_include_directories(dm_plugin_host PRIVATE src)
target_compile_options(dm_plugin_host PRIVATE /W3 /utf-8)

# ============================================================
# 主进程
# ============================================================
add_executable(dm_browser
    src/db/Database.cpp
    src/ipc/Pipe.cpp
    src/ipc/Framing.cpp
    src/ProcessManager.cpp
    src/SiteIsolator.cpp
    src/Sandbox.cpp
    src/IPCBroker.cpp
    src/main.cpp
)
target_include_directories(dm_browser PRIVATE src)
target_link_libraries(dm_browser PRIVATE sqlite3)
target_compile_options(dm_browser PRIVATE /W3 /utf-8)

# ============================================================
# Windows 专有：UI + 示例插件
# ============================================================
if(WIN32)
    target_link_libraries(dm_browser PRIVATE userenv)

    add_executable(dm_ui
        src/ui/Window.cpp
        src/ui/ui_main.cpp
    )
    target_include_directories(dm_ui PRIVATE src)
    target_compile_options(dm_ui PRIVATE /W3 /utf-8)
    target_link_libraries(dm_ui PRIVATE comctl32 user32 gdi32)

    add_library(example_plugin SHARED
        src/plugin_sdk/example_plugin.cpp
    )
    target_include_directories(example_plugin PRIVATE src)
    set_target_properties(example_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "example_plugin"
    )
    target_compile_options(example_plugin PRIVATE /W3 /utf-8)
endif()

# ============================================================
# 独立测试套件
# ============================================================
add_executable(dm_tests
    src/db/Database.cpp
    src/ipc/Pipe.cpp
    src/ipc/Framing.cpp
    src/ProcessManager.cpp
    src/SiteIsolator.cpp
    src/Sandbox.cpp
    src/IPCBroker.cpp
    tests/test_main.cpp
    tests/test_milestone1.cpp
    tests/test_milestone2.cpp
    tests/test_milestone3.cpp
    tests/test_milestone4.cpp
    tests/test_milestone5.cpp
    tests/test_milestone6.cpp
    tests/test_ipc.cpp
    tests/test_persistence.cpp
)
target_include_directories(dm_tests PRIVATE src tests)
target_link_libraries(dm_tests PRIVATE sqlite3)
target_compile_options(dm_tests PRIVATE /W3 /utf-8)