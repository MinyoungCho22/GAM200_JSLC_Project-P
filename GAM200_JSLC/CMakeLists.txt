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

# 4. MSVC Options
if(MSVC)
    add_compile_options(/utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
endif()

# ==========================================================
# [Include & Link Settings] (완전 독립형 설정)
# ==========================================================

# 1. Include Paths
# vcpkg 경로를 제거하고, 프로젝트 내 'include' 폴더를 추가했습니다.
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Game"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenGL"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS200"
    "${CMAKE_CURRENT_SOURCE_DIR}/include" # [중요] 여기에 헤더 파일들을 넣어주세요!
)

# 2. Library Search Paths
# 프로젝트 내 'lib' 폴더를 라이브러리 검색 경로로 지정합니다.
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 3. fmod_vc.lib 복사 (Engine 폴더에 있는 경우)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Engine/fmod_vc.lib" 
     DESTINATION "${CMAKE_BINARY_DIR}")

# 4. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    "${CMAKE_BINARY_DIR}/fmod_vc.lib"
    opengl32 
    glew32 
    SDL2
    SDL2main
)

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

# ==========================================================
# [Visual Studio Filter Settings]
# ==========================================================
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${HEADERS})
source_group("OpenGL\\Shaders" FILES ${SHADER_FILES})