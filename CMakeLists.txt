cmake_minimum_required(VERSION 3.16)
project(guitarist_sorcerer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(guitarist_sorcerer
    src/main.cpp
    src/player.cpp
    src/chord_system.cpp
    src/enemy.cpp
)
set_target_properties(guitarist_sorcerer PROPERTIES OUTPUT_NAME "MagicPick")

if(APPLE)
    # Link raylib statically so the packaged binary doesn't depend on a
    # Homebrew install at a fixed path on whichever Mac it's copied to.
    execute_process(
        COMMAND brew --prefix raylib
        OUTPUT_VARIABLE RAYLIB_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    find_library(RAYLIB_STATIC_LIB NAMES libraylib.a PATHS "${RAYLIB_PREFIX}/lib" NO_DEFAULT_PATH)
    target_include_directories(guitarist_sorcerer PRIVATE "${RAYLIB_PREFIX}/include")
    target_link_libraries(guitarist_sorcerer
        "${RAYLIB_STATIC_LIB}"
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
else()
    find_package(raylib REQUIRED)
    target_link_libraries(guitarist_sorcerer raylib)
endif()

# Keep assets next to the executable in the build dir too, so running from
# the build folder behaves the same as running the packaged distribution.
add_custom_command(TARGET guitarist_sorcerer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:guitarist_sorcerer>/assets"
)
