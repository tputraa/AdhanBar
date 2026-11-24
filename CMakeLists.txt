cmake_minimum_required(VERSION 3.20)
project(AdhanBar LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "AdhanBar requires Windows.")
endif()

enable_language(RC)

add_executable(AdhanBar WIN32 AdhanBar.cpp app.rc)
target_compile_features(AdhanBar PRIVATE cxx_std_17)
target_compile_definitions(AdhanBar PRIVATE _WIN32_WINNT=0x0A00)
target_link_libraries(AdhanBar PRIVATE comdlg32 gdi32 shell32 user32 winhttp)

if(MSVC)
    target_compile_options(AdhanBar PRIVATE /W4 /EHsc)
else()
    target_compile_options(AdhanBar PRIVATE -Wall -Wextra)
endif()
