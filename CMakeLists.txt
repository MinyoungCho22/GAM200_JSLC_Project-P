cmake_minimum_required(VERSION 3.20)

project(GAM200_JSLC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio에서 폴더 구조로 파일 표시
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/OpenGL/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/OpenGL/*.hpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Visual Studio에서 원본 폴더 구조로 파일 그룹화
foreach(SOURCE_FILE ${PROJECT_SOURCES})
    # 상대 경로 계산
    file(RELATIVE_PATH RELATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${SOURCE_FILE})
    # 디렉토리 경로 추출
    get_filename_component(SOURCE_DIR ${RELATIVE_PATH} DIRECTORY)
    # 슬래시를 백슬래시로 변환 (Visual Studio 형식)
    if(SOURCE_DIR)
        string(REPLACE "/" "\\" SOURCE_DIR ${SOURCE_DIR})
        source_group("${SOURCE_DIR}" FILES ${SOURCE_FILE})
    else()
        source_group("" FILES ${SOURCE_FILE})
    endif()
endforeach()

# Set working directory for Visual Studio debugger
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC
)

find_package(SDL2 CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    GLEW::GLEW
    glfw
    OpenGL::GL
)

find_path(STB_INCLUDE_DIR stb_image.h
    HINTS
        "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
        "C:/vcpkg/installed/x64-windows/include"
)

if(STB_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIR})
else()
    message(WARNING "stb_image.h을 찾을 수 없습니다. vcpkg toolchain을 설정했는지 확인하세요.")
endif()

# FMOD 라이브러리 설정
set(FMOD_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/Engine")
set(FMOD_LIB "${FMOD_LIB_DIR}/fmod_vc.lib")
if(EXISTS "${FMOD_LIB}")
    target_link_directories(${PROJECT_NAME} PRIVATE "${FMOD_LIB_DIR}")
    target_link_libraries(${PROJECT_NAME} PRIVATE fmod_vc.lib)
    # CMake 빌드임을 표시 (Sound.cpp에서 pragma comment를 비활성화하기 위해)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CMAKE_BUILD)
else()
    message(WARNING "FMOD 라이브러리(fmod_vc.lib)를 찾을 수 없습니다: ${FMOD_LIB}")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/fmod.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/Asset
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/GAM200_JSLC/OpenGL/Shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/OpenGL/Shaders
)
