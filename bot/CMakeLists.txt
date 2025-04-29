cmake_minimum_required(VERSION 3.15)
project(discord-bot VERSION 1.0 DESCRIPTION "A Discord bot using DPP library")

# Required for DPP and modern C++ features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configure DPP library (assuming you have it installed via Nix)
find_package(dpp REQUIRED
    HINTS ${DPP_INCLUDE_DIR}  # Add path if needed: /nix/store/.../include
)
file(GLOB_RECURSE SRC_FILES
    include/*.cpp
    include/*.hpp
)
# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${SRC_FILES}
    # Add other source files here
)


# Link libraries with proper dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    dpp
    OpenSSL::SSL
    OpenSSL::Crypto
    z
    opus
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${DPP_INCLUDE_DIRS}
)

# macOS ARM64 specific fixes
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -arch arm64
        -I/opt/homebrew/opt/openssl/include  # If using Homebrew
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -arch arm64
        -L/opt/homebrew/opt/openssl/lib      # If using Homebrew
    )
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -arch x86_64
        -I/opt/homebrew/opt/openssl/include  # If using Homebrew
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -arch x86_64
        -L/opt/homebrew/opt/openssl/lib      # If using Homebrew
    )
elseif(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -m64
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -m64
    )
elseif(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -marm
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -marm
    )
endif()
