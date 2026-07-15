cmake_minimum_required(VERSION 3.16)
project(guitarist_sorcerer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IS_IOS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
endif()

if(IS_IOS)
    # iOS has no prebuilt raylib package, so build it from source. See
    # vendor/raylib-ios for the (unofficial, unmerged) iOS platform fork.
    set(PLATFORM "iOS" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "" FORCE)
    # raudio.c doesn't compile yet on this iOS fork (miniaudio's AVFoundation
    # backend needs Objective-C compilation, which isn't wired up here) --
    # excluded for now; src/ios_audio_stub.cpp stands in so the game still
    # links and runs, just silently. Revisit once real iOS audio is tackled.
    # CUSTOMIZE_BUILD must be ON, or raylib's cmake_dependent_option() logic
    # silently forces SUPPORT_MODULE_RAUDIO back to its default (ON).
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/raylib-ios raylib-ios-build)

    add_executable(guitarist_sorcerer MACOSX_BUNDLE
        src/main.cpp
        src/player.cpp
        src/chord_system.cpp
        src/enemy.cpp
        src/ios_audio_stub.cpp
        src/touch_controls.cpp
    )
    set_target_properties(guitarist_sorcerer PROPERTIES
        OUTPUT_NAME "MagicPick"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ardaunver.magicpick"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        # Custom Info.plist: the default one CMake generates has no way to
        # lock orientation. This game is landscape-only, matching its 16:9
        # virtual canvas -- without this the device defaults to portrait and
        # squeezes the game into a thin strip in the middle of the screen.
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ardaunver.magicpick"
        # This machine's macOS/Xcode combo tags every file (even ones Xcode
        # generates itself mid-build, like Info.plist) with a com.apple.provenance
        # extended attribute that codesign rejects outright. Simulator builds
        # don't need real signing, so skip it entirely rather than fight that.
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )
    target_link_libraries(guitarist_sorcerer
        raylib
        "-framework UIKit"
        "-framework QuartzCore"
        "-framework OpenGLES"
        "-framework CoreMotion"
        "-framework Foundation"
    )

    # Bundle assets as resources so they end up inside MagicPick.app instead
    # of next to it (iOS apps can only read from within their own bundle).
    # .DS_Store and other Finder detritus must be excluded -- codesign
    # refuses to sign a bundle containing them ("resource fork, Finder
    # information, or similar detritus not allowed").
    file(GLOB_RECURSE MAGICPICK_ASSET_FILES RELATIVE "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_SOURCE_DIR}/assets/*")
    list(FILTER MAGICPICK_ASSET_FILES EXCLUDE REGEX "(^|/)\\.[^/]*$")
    foreach(ASSET_FILE ${MAGICPICK_ASSET_FILES})
        get_filename_component(ASSET_DIR "${ASSET_FILE}" DIRECTORY)
        target_sources(guitarist_sorcerer PRIVATE "${CMAKE_SOURCE_DIR}/assets/${ASSET_FILE}")
        set_source_files_properties("${CMAKE_SOURCE_DIR}/assets/${ASSET_FILE}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "assets/${ASSET_DIR}")
    endforeach()
else()
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
endif()
