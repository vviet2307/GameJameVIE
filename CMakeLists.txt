# ============================================================
#  Au fil de la vie  ·  CMakeLists.txt
#
#  Desktop build for the original 2D jam game.
#  raylib is auto-fetched if not installed.
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(AuFilDeLaVie CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── raylib ───────────────────────────────────────────────────
find_package(raylib 4.0 QUIET)

if(NOT raylib_FOUND)
    message(STATUS "raylib not found locally — fetching via FetchContent")
    include(FetchContent)
    # raylib 5.0 still declares an old cmake_minimum_required; CMake 4 needs
    # this policy override so the sub-project configures.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.0
        GIT_SHALLOW    TRUE
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

# ── target ───────────────────────────────────────────────────
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Apple Silicon / macOS frameworks
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# Linux extras
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        m pthread dl
    )
endif()
