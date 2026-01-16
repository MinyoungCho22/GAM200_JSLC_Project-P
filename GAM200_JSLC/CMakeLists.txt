cmake_minimum_required(VERSION 3.16)

# 1. Project Name
project(GAM200JSLC VERSION 1.0 LANGUAGES CXX)

# 2. C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(/utf-8)

# ==========================================================
# [Collect Source/Header/Shader Files]
# ==========================================================
# Source files (.cpp)
# [FIX] Added CONFIGURE_DEPENDS: Automatically re-configure if files change
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

# [Added] Shader files (.vert, .frag) - Collected to display in VS
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "OpenGL/Shaders/*.vert"
    "OpenGL/Shaders/*.frag"
)

# 3. Create Executable (Include shader files here to show them in IDE)
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${SHADER_FILES})

# 4. MSVC Options
if(MSVC)
    add_compile_options(/utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
endif()

# 5. Include Paths
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Game"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenGL"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS200"
    "C:/vcpkg/installed/x64-windows/include"
)

# ==========================================================
# [Library Settings]
# ==========================================================
# 1. Force copy 'fmod_vc.lib' to build directory (Kept existing logic)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Engine/fmod_vc.lib" 
     DESTINATION "${CMAKE_BINARY_DIR}")

# 2. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    "${CMAKE_BINARY_DIR}/fmod_vc.lib"  # Use the copied lib
    opengl32 
    glew32 
    SDL2
)

# ==========================================================
# [Post-Build Events (Auto Copy)]
# ==========================================================

# 1. Copy 'Asset' folder
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Asset
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset
    COMMENT "Copying Asset folder..."
)

# 2. Copy 'OpenGL' folder entirely (Solves shader path issues)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/OpenGL
    COMMENT "Copying OpenGL folder..."
)

# [Added] 3. Auto Copy DLL Files (DLLs folder -> Next to Executable)
# Find all .dll files in the 'DLLs' folder of the project root.
# [FIX] Added CONFIGURE_DEPENDS ensures CMake sees new DLLs immediately.
file(GLOB DLL_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DLLs/*.dll")

# Copy found DLLs iteratively
foreach(DLL ${DLL_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLL}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying DLL: ${DLL}"
    )
endforeach()

# ==========================================================
# [Visual Studio Filter Settings]
# ==========================================================
# Organize standard source codes by folder structure
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${HEADERS})

# [Added] Organize shader files neatly under 'OpenGL/Shaders' filter
source_group("OpenGL\\Shaders" FILES ${SHADER_FILES})