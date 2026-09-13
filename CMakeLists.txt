cmake_minimum_required(VERSION 3.20)
project(dm_browser C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(DM_PLATFORM_WINDOWS)
endif()

if(WIN32)
    set(WEBVIEW2_VERSION "1.0.2903.40")
    set(WEBVIEW2_URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}")
    set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2")
    set(WEBVIEW2_HEADER "${WEBVIEW2_DIR}/build/native/include/WebView2.h")

    if(NOT EXISTS "${WEBVIEW2_HEADER}")
        message(STATUS "Downloading WebView2 SDK ${WEBVIEW2_VERSION} ...")
        set(WEBVIEW2_NUPKG "${CMAKE_BINARY_DIR}/webview2.nupkg")
        file(DOWNLOAD "${WEBVIEW2_URL}" "${WEBVIEW2_NUPKG}"
             STATUS wv2_status SHOW_PROGRESS)
        list(GET wv2_status 0 wv2_code)
        if(NOT wv2_code EQUAL 0)
            message(FATAL_ERROR "WebView2 SDK download failed")
        endif()
        file(MAKE_DIRECTORY "${WEBVIEW2_DIR}")
        file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_NUPKG}" DESTINATION "${WEBVIEW2_DIR}")
        if(NOT EXISTS "${WEBVIEW2_HEADER}")
            message(FATAL_ERROR "WebView2 header missing after extract")
        endif()
    endif()

    set(WEBVIEW2_INCLUDE "${WEBVIEW2_DIR}/build/native/include")
    set(WEBVIEW2_LIB "${WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib")
endif()

add_library(sqlite3 STATIC
    third_party/sqlite3/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party/sqlite3)
target_compile_options(sqlite3 PRIVATE /w /utf-8)

add_executable(dm_plugin_host
    src/ipc/Pipe.cpp
    src/ipc/Framing.cpp
    src/plugin_host/plugin_host_main.cpp
)
target_include_directories(dm_plugin_host PRIVATE src)
target_compile_options(dm_plugin_host PRIVATE /W3 /utf-8)

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

if(WIN32)
    target_link_libraries(dm_browser PRIVATE userenv)

    # --- UI：HTML/CSS 双层 WebView2 ---
    add_executable(dm_ui
        src/ui/BrowserWindow.cpp
        src/ui/TabManager.cpp
        src/ui/ui_main.cpp
    )
    target_include_directories(dm_ui PRIVATE src "${WEBVIEW2_INCLUDE}")
    target_link_libraries(dm_ui PRIVATE
        "${WEBVIEW2_LIB}"
        shlwapi ole32 oleaut32
    )
    target_compile_options(dm_ui PRIVATE /W3 /utf-8)

    # 复制 UI 资源到输出目录
    add_custom_command(TARGET dm_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/ui/ui.html"
            "$<TARGET_FILE_DIR:dm_ui>/ui.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/ui/start_page.html"
            "$<TARGET_FILE_DIR:dm_ui>/start_page.html"
        COMMENT "复制 UI 资源到输出目录"
    )

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