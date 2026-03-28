cmake_minimum_required(VERSION 3.21)

project(GDBrowser VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

file(GLOB_RECURSE GD_BROWSER_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${GD_BROWSER_SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK or run `geode sdk install` / `geode sdk set-path`.")
else()
    message(STATUS "Found Geode SDK: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(${PROJECT_NAME} Comdlg32)
endif()

set(GD_BROWSER_WEBVIEW2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2")

if (WIN32)
    set(WEBVIEW2_INCLUDE_DIR "${GD_BROWSER_WEBVIEW2_DIR}/include")
    set(WEBVIEW2_STATIC_LIB "${GD_BROWSER_WEBVIEW2_DIR}/lib/x64/WebView2LoaderStatic.lib")

    if (EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" AND EXISTS "${WEBVIEW2_STATIC_LIB}")
        message(STATUS "Using vendored WebView2 SDK from ${GD_BROWSER_WEBVIEW2_DIR}")
        target_include_directories(${PROJECT_NAME} PRIVATE "${WEBVIEW2_INCLUDE_DIR}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE GD_BROWSER_HAS_WEBVIEW2=1)
        target_link_libraries(${PROJECT_NAME} "${WEBVIEW2_STATIC_LIB}" version)
    else()
        message(WARNING "Vendored WebView2 SDK not found; building scaffold backend instead")
    endif()
endif()

setup_geode_mod(${PROJECT_NAME})
