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

    add_executable(dm_ui
        src/db/Database.cpp
        src/ui/BrowserWindow.cpp
        src/ui/TabManager.cpp
        src/ui/ui_main.cpp
        src/ui/AiClient.cpp
        src/learn/learn_store.cpp
        src/learn/snapshot_parser.cpp
        src/learn/multi_compare.cpp
    )
    target_include_directories(dm_ui PRIVATE src "${WEBVIEW2_INCLUDE}")
    target_link_libraries(dm_ui PRIVATE
        sqlite3
        "${WEBVIEW2_LIB}"
        shlwapi ole32 oleaut32
        winhttp
    )
    target_compile_options(dm_ui PRIVATE /W3 /utf-8)

    add_custom_command(TARGET dm_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/ui/ui.html"
            "$<TARGET_FILE_DIR:dm_ui>/ui.html"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/ui/sidebar.html"
            "$<TARGET_FILE_DIR:dm_ui>/sidebar.html"
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
        PREFIX "" OUTPUT_NAME "example_plugin")
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

# ============================================================
# dm_render_test — 自研渲染引擎（批 D 全部功能）
# ============================================================
if(WIN32)
    add_executable(dm_render_test
        src/render/main.cpp
        src/render/html_parser.cpp
        src/render/css_parser.cpp
        src/render/style_resolver.cpp
        src/render/layout_engine.cpp
        src/render/snapshot_dumper.cpp
        src/render/render_window.cpp
        src/learn/snapshot_parser.cpp
    )
    target_include_directories(dm_render_test PRIVATE
        src
        third_party
        third_party/nlohmann
    )
    target_compile_options(dm_render_test PRIVATE /W3 /utf-8)
    target_link_libraries(dm_render_test PRIVATE user32 gdi32)
endif()

# ============================================================
# dm_kb — 知识库工具
# ============================================================
add_executable(dm_kb
    src/knowledge/kb_main.cpp
    src/knowledge/knowledge_base.cpp
)
target_include_directories(dm_kb PRIVATE
    src
    src/knowledge
    third_party
    third_party/nlohmann
)
target_compile_options(dm_kb PRIVATE /W3 /utf-8)
target_link_libraries(dm_kb PRIVATE user32)

add_custom_command(TARGET dm_kb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:dm_kb>/knowledge"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/knowledge/html.json"
        "$<TARGET_FILE_DIR:dm_kb>/knowledge/html.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/knowledge/css.json"
        "$<TARGET_FILE_DIR:dm_kb>/knowledge/css.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/knowledge/js.json"
        "$<TARGET_FILE_DIR:dm_kb>/knowledge/js.json"
    COMMENT "复制知识库到输出目录"
)

# ============================================================
# dm_learn — 学习库工具
# ============================================================
add_executable(dm_learn
    src/learn/learn_main.cpp
    src/learn/learn_store.cpp
    src/learn/snapshot_parser.cpp
    src/learn/multi_compare.cpp
    src/db/Database.cpp
)
target_include_directories(dm_learn PRIVATE
    src
    third_party
    third_party/nlohmann
)
target_compile_options(dm_learn PRIVATE /W3 /utf-8)
target_link_libraries(dm_learn PRIVATE sqlite3 user32)

# ============================================================
# 安装规则
# ============================================================
install(TARGETS dm_browser dm_plugin_host dm_tests dm_ui example_plugin
        RUNTIME DESTINATION bin)
install(FILES
        "${CMAKE_SOURCE_DIR}/src/ui/ui.html"
        "${CMAKE_SOURCE_DIR}/src/ui/sidebar.html"
        "${CMAKE_SOURCE_DIR}/src/ui/start_page.html"
        DESTINATION bin)

# ============================================================
# CPack 打包
# ============================================================
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "dm-browser")
set(CPACK_PACKAGE_VERSION "0.3.0")
set(CPACK_PACKAGE_DESCRIPTION "大明DM浏览器")
set(CPACK_PACKAGE_FILE_NAME "dm-browser-${CPACK_PACKAGE_VERSION}-win64")
include(CPack)