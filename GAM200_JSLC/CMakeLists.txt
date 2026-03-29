cmake_minimum_required(VERSION 3.16)

# 1. Project Name
project(GAM200JSLC VERSION 1.0 LANGUAGES CXX)

# 2. C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_FMOD "Enable FMOD audio if platform libraries are available" ON)

# ==========================================================
# [Collect Source/Header/Shader Files]
# ==========================================================
# Source files (.cpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "main.cpp"
    "Engine/*.cpp"
    "Game/*.cpp"
    "OpenGL/*.cpp"
    "CS200/*.cpp"
    "ThirdParty/imgui/*.cpp"
    "ThirdParty/imgui/backends/*.cpp"
)

# Header files (.hpp, .h)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "Engine/*.hpp"
    "Engine/*.h"
    "Game/*.hpp"
    "OpenGL/*.hpp"
    "CS200/*.hpp"
    "CS200/*.h"
)

# Shader files (.vert, .frag)
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "OpenGL/Shaders/*.vert"
    "OpenGL/Shaders/*.frag"
)

# 3. Create Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${SHADER_FILES})

# Windows: product name + icon (Robot_High.png → Win32/app_icon.ico via Win32/RefreshAppIcon.ps1; embedded by app.rc)
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "No. 99"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/Release"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/Release"
    )
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Win32/app.rc")
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/Win32/app.rc"
        PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Win32/app_icon.ico"
    )
    # Regenerate ICO before compile so Release always embeds latest Robot_High.png (and rc recompiles).
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/Win32/RefreshAppIcon.ps1"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Refresh app_icon.ico from Asset/Robot_High.png"
    )
endif()

# 4. Compiler/Platform Options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE USE_GLEW)
else()
    # Silence unused-result errors from system headers on Unix-likes if needed
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    if(APPLE)
        # Silence macOS OpenGL deprecation warnings
        target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
        # Ensure C++ stdlib headers are found (SDK-based toolchain on macOS)
        set(OSX_SYSROOT "$ENV{SDKROOT}")
        if(NOT OSX_SYSROOT)
            set(OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")
        endif()
        target_include_directories(${PROJECT_NAME} PRIVATE SYSTEM "${OSX_SYSROOT}/usr/include/c++/v1")
    endif()
endif()

# ==========================================================
# [Include & Link Settings]
# ==========================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Game"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenGL"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS200"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/json"
)

# Dear ImGui OpenGL backend needs to know which GL loader is used by the application.
# This project uses GLEW on Windows (see USE_GLEW).
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW SDL_MAIN_HANDLED)

find_package(OpenGL REQUIRED)
find_package(PkgConfig QUIET)

# SDL2
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND AND PKG_CONFIG_FOUND AND NOT WIN32)
    pkg_check_modules(SDL2 sdl2)
endif()

if(SDL2_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
elseif(SDL2_LIBRARIES)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
endif()

# GLEW (only required on Windows where bundled binaries exist)
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# GLFW
if(NOT WIN32)
    find_package(glfw3 CONFIG QUIET)
    if(NOT glfw3_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(GLFW glfw3)
    endif()

    if(glfw3_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    elseif(GLFW_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
    endif()
endif()

if(WIN32)
    # Additional Windows-only search paths and libraries
    target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
        glew32
        SDL2
        glfw3dll
    )
endif()

if(ENABLE_FMOD)
    if(WIN32)
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Engine/fmod_vc.lib"
             DESTINATION "${CMAKE_BINARY_DIR}")
        target_link_libraries(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/fmod_vc.lib")
    elseif(APPLE)
        set(FMOD_DYLIB "${CMAKE_CURRENT_SOURCE_DIR}/Engine/libfmod.dylib")
        if(EXISTS "${FMOD_DYLIB}")
            target_link_libraries(${PROJECT_NAME} PRIVATE "${FMOD_DYLIB}")

            # Ensure the dylib is next to the executable at runtime
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FMOD_DYLIB}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
                COMMENT "Copying FMOD dylib..."
            )

            # Make the executable search for dylibs next to it
            set_target_properties(${PROJECT_NAME} PROPERTIES
                BUILD_RPATH "@executable_path"
            )
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_FMOD)
            message(WARNING "FMOD enabled but ${FMOD_DYLIB} not found; building with DISABLE_FMOD")
        endif()
    else()
        # Other non-Windows platforms not configured for FMOD in this repo
        target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_FMOD)
        message(WARNING "FMOD enabled but not configured for this platform; building with DISABLE_FMOD")
    endif()
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_FMOD)
endif()

# GLEW cannot be delay-loaded (LNK1194: data symbol imports). glew32.dll stays next to the exe.
# GLFW / SDL2 / FMOD delay-load so they can be resolved from bin/ after SetDllDirectory in main().
if(WIN32 AND MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        /DELAYLOAD:glfw3.dll
        /DELAYLOAD:SDL2.dll
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE delayimp.lib)
    if(ENABLE_FMOD)
        target_link_options(${PROJECT_NAME} PRIVATE /DELAYLOAD:fmod.dll)
    endif()
endif()

# ==========================================================
# [Post-Build Events (Auto Copy)]
# ==========================================================

# 1. Copy Asset
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Asset
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset
    COMMENT "Copying Asset folder..."
)

# 2. Copy OpenGL shaders (runtime path: OpenGL/Shaders/...)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL/Shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/OpenGL/Shaders
    COMMENT "Copying OpenGL/Shaders..."
)

if(WIN32)
    # bin/: GLFW/SDL/FMOD (delay-loaded). Exe dir: glew32.dll (must load before main; GLEW cannot use /DELAYLOAD).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DLLs")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/bin"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/DLLs"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/bin"
            COMMENT "Copying DLLs to bin/..."
        )
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DLLs/glew32.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/DLLs/glew32.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/glew32.dll"
            COMMENT "Copying glew32.dll next to exe..."
        )
    endif()

    # Drop stale exe from before OUTPUT_NAME was set to "No. 99"
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/GAM200JSLC.exe"
        COMMENT "Removing stale GAM200JSLC.exe if present..."
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Config")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Config"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Config"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/Asset/config/map_objects.json"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Config/map_objects.json"
            COMMENT "Copying Config JSON..."
        )
    endif()

    # Remove applocal copies for DLLs that load from bin/ (glew32 stays next to exe).
    if(MSVC)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/glfw3.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fmod.dll"
            COMMENT "Removing glfw/SDL/fmod next to exe (loaded from bin/)..."
        )
    endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

# ==========================================================
# [Visual Studio Filter Settings]
# ==========================================================
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${HEADERS})
source_group("OpenGL\\Shaders" FILES ${SHADER_FILES})