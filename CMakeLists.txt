cmake_minimum_required(VERSION 3.14)
project(TCPChat VERSION 1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_SERVER "Build the chat server" ON)
option(BUILD_CLIENT "Build the chat client (requires Qt)" ON)
option(BUILD_SERVER_GUI "Build the chat server with GUI (requires Qt and SQLite3)" ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread)
endif()

# Common library (Protocol)
add_library(common STATIC
    common/Protocol.cpp
    common/Protocol.h
)
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

# Server
set(SERVER_BUILT FALSE)
if(BUILD_SERVER)
    message(STATUS "Building Chat Server...")

    # Find SQLite3 - multiple methods
    find_package(SQLite3 QUIET)

    if(NOT SQLite3_FOUND)
        # Try pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SQLITE3 QUIET sqlite3)
        endif()
    endif()

    # Try to find sqlite3 header directly
    if(NOT SQLite3_FOUND AND NOT SQLITE3_FOUND)
        find_path(SQLITE3_INCLUDE_DIR sqlite3.h
            PATHS /usr/include /usr/local/include
        )
        find_library(SQLITE3_LIBRARY sqlite3
            PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )
        if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
            set(SQLITE3_FOUND TRUE)
            set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
            set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
        endif()
    endif()

    if(SQLite3_FOUND OR SQLITE3_FOUND)
        add_executable(chat_server
            server/server_main.cpp
            server/Server.cpp
            server/Server.h
            server/ClientSession.cpp
            server/ClientSession.h
            server/Database.cpp
            server/Database.h
        )

        target_link_libraries(chat_server
            common
            ${PLATFORM_LIBS}
        )

        if(SQLite3_FOUND)
            target_include_directories(chat_server PRIVATE ${SQLite3_INCLUDE_DIRS})
            target_link_libraries(chat_server ${SQLite3_LIBRARIES})
        else()
            target_include_directories(chat_server PRIVATE ${SQLITE3_INCLUDE_DIRS})
            target_link_libraries(chat_server ${SQLITE3_LIBRARIES})
        endif()

        set(SERVER_BUILT TRUE)
        message(STATUS "Chat Server will be built")
    else()
        message(WARNING "SQLite3 not found. Server will not be built.")
        message(WARNING "Install SQLite3: sudo apt install libsqlite3-dev (Ubuntu/Debian)")
    endif()
endif()

# Client
set(CLIENT_BUILT FALSE)
if(BUILD_CLIENT)
    message(STATUS "Building Chat Client...")

    # Find Qt (prefer Qt6, fall back to Qt5)
    find_package(Qt6 COMPONENTS Widgets Network QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets Network QUIET)
        if(Qt5_FOUND)
            set(QT_VERSION 5)
            set(QT_LIBRARIES Qt5::Widgets Qt5::Network)
        endif()
    else()
        set(QT_VERSION 6)
        set(QT_LIBRARIES Qt6::Widgets Qt6::Network)
    endif()

    if(Qt6_FOUND OR Qt5_FOUND)
        # Enable Qt MOC, UIC, RCC
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)

        add_executable(chat_client
            client/client_main.cpp
            client/MainWindow.cpp
            client/MainWindow.h
            client/AuthDialog.cpp
            client/AuthDialog.h
            client/ChatClient.cpp
            client/ChatClient.h
        )

        target_link_libraries(chat_client
            common
            ${QT_LIBRARIES}
            ${PLATFORM_LIBS}
        )

        set(CLIENT_BUILT TRUE)
        message(STATUS "Chat Client will be built with Qt${QT_VERSION}")
    else()
        message(WARNING "Qt5 or Qt6 not found. Client will not be built.")
        message(WARNING "Install Qt5: sudo apt install qtbase5-dev (Ubuntu/Debian)")
        message(WARNING "Install Qt6: sudo apt install qt6-base-dev (Ubuntu 22.04+)")
    endif()
endif()

# Server GUI (requires both Qt and SQLite3)
set(SERVER_GUI_BUILT FALSE)
if(BUILD_SERVER_GUI)
    message(STATUS "Building Chat Server GUI...")

    # Check if we have both Qt and SQLite3
    if((Qt6_FOUND OR Qt5_FOUND) AND (SQLite3_FOUND OR SQLITE3_FOUND))
        # Enable Qt MOC if not already enabled
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)

        add_executable(chat_server_gui
            server_gui/server_gui_main.cpp
            server_gui/ServerWindow.cpp
            server_gui/ServerWindow.h
            server/Server.cpp
            server/Server.h
            server/ClientSession.cpp
            server/ClientSession.h
            server/Database.cpp
            server/Database.h
        )

        target_link_libraries(chat_server_gui
            common
            ${QT_LIBRARIES}
            ${PLATFORM_LIBS}
        )

        if(SQLite3_FOUND)
            target_include_directories(chat_server_gui PRIVATE ${SQLite3_INCLUDE_DIRS})
            target_link_libraries(chat_server_gui ${SQLite3_LIBRARIES})
        else()
            target_include_directories(chat_server_gui PRIVATE ${SQLITE3_INCLUDE_DIRS})
            target_link_libraries(chat_server_gui ${SQLITE3_LIBRARIES})
        endif()

        set(SERVER_GUI_BUILT TRUE)
        message(STATUS "Chat Server GUI will be built with Qt${QT_VERSION}")
    else()
        if(NOT (Qt6_FOUND OR Qt5_FOUND))
            message(WARNING "Qt not found. Server GUI will not be built.")
        endif()
        if(NOT (SQLite3_FOUND OR SQLITE3_FOUND))
            message(WARNING "SQLite3 not found. Server GUI will not be built.")
        endif()
    endif()
endif()

# Installation (only for targets that exist)
if(SERVER_BUILT)
    install(TARGETS chat_server RUNTIME DESTINATION bin)
endif()
if(CLIENT_BUILT)
    install(TARGETS chat_client RUNTIME DESTINATION bin)
endif()
if(SERVER_GUI_BUILT)
    install(TARGETS chat_server_gui RUNTIME DESTINATION bin)
endif()

# Print summary
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
