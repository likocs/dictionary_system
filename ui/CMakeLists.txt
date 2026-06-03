cmake_minimum_required(VERSION 3.16)
project(dictionary_ui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
qt_standard_project_setup()

qt_add_executable(ex01
    hello.cpp
    dic_client.cpp
    dic_client.hpp
    dic_protocol.hpp
    qwen_client.cpp
    qwen_client.hpp
    chat_dialog.cpp
    chat_dialog.hpp
    desktop_pet_widget.cpp
    desktop_pet_widget.hpp
    login_window.cpp
    login_window.hpp
    main_window.cpp
    main_window.hpp
)
target_link_libraries(ex01 PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

set_target_properties(ex01 PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
