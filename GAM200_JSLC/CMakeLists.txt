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

# 4. Compiler/Platform Options
if(MSVC)
    add_compile_options(/utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
else()
    # Silence unused-result errors from system headers on Unix-likes if needed
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    if(APPLE)
        # Silence macOS OpenGL deprecation warnings
        target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()

# ==========================================================
# [Include & Link Settings]
# ==========================================================

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Game"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenGL"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS200"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

find_package(OpenGL REQUIRED)
find_package(PkgConfig QUIET)

# SDL2
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND AND PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

if(SDL2_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
endif()

# GLEW (only required on Windows where bundled binaries exist)
if(WIN32)
    find_package(GLEW CONFIG QUIET)
    if(NOT GLEW_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(GLEW REQUIRED glew)
    endif()

    if(GLEW_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW)
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE ${GLEW_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${GLEW_LIBRARIES})
    endif()

    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

# GLFW
find_package(glfw3 CONFIG QUIET)
if(NOT glfw3_FOUND AND PKG_CONFIG_FOUND)
    # Homebrew usually installs pkg-config file as glfw3.pc
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

if(glfw3_FOUND)
    # CMake config for glfw3 typically provides a target named glfw
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
elseif(GLFW_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
endif()

if(WIN32)
    # Additional Windows-only search paths and libraries
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
        glew32
        SDL2
        SDL2main
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

# 2. Copy OpenGL
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/OpenGL
    COMMENT "Copying OpenGL folder..."
)

if(WIN32)
    # 3. Auto Copy DLL Files
    file(GLOB DLL_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DLLs/*.dll")

    foreach(DLL ${DLL_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DLL}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying DLL: ${DLL}"
        )
    endforeach()
endif()

# ==========================================================
# [Visual Studio Filter Settings]
# ==========================================================
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${HEADERS})
source_group("OpenGL\\Shaders" FILES ${SHADER_FILES})