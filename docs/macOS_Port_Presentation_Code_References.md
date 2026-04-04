# macOS

Windows 중심 설계를 유지하면서 macOS에서도 동일한 UX를 맞추기 위한 **CMake 빌드**, **OpenGL 4.1 파이프라인**, **이벤트·렌더링 보정** 코드 위치를 정리했습니다. (경로는 저장소 기준 `GAM200_JSLC/`)



## 요약

- **GLEW 없이** macOS에서는 `USE_GLEW`가 켜지지 않아 `glewInit`을 건너뛰고, 시스템이 제공하는 OpenGL 엔트리포인트를 사용합니다.
- **Retina**에서는 논리 해상도(가상 1920×1080)로 씬을 그리고, **매 프레임 실제 프레임버퍼 픽셀 크기**를 읽어 뷰포트·포스트 셰이더에 반영합니다.
- **전체화면/창 전환** 등 타이밍 이슈는 `glfwPollEvents` 추가 호출과 **프레임버퍼 크기 재질의**로 보정합니다.

---

## 1. 하이브리드 빌드 시스템 (CMake)

### 1.1 플랫폼 자동 감지 — Windows는 GLEW, macOS는 GLEW 미사용

- **Windows(MSVC)**: `USE_GLEW` 정의 + `glew32` 링크.
- **Linux 등 (`UNIX AND NOT APPLE`)**: 시스템 GLEW 검색 후 링크.
- **macOS**: 아래 분기에 들어가지 않으므로 `**USE_GLEW`가 정의되지 않음** → `Engine.cpp`의 `#ifdef USE_GLEW` 블록(`glewInit`)이 생략됩니다.

`*GAM200_JSLC/CMakeLists.txt` (약 130–154행)*

```cmake
# GLEW: Windows uses bundled DLL; Linux/*BSD need system GLEW for core GL entry points.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW)
elseif(UNIX AND NOT APPLE)
    find_package(GLEW QUIET)
    if(NOT GLEW_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(PC_GLEW QUIET glew)
    endif()
    if(GLEW_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW)
        if(TARGET GLEW::GLEW)
            target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW)
        else()
            target_include_directories(${PROJECT_NAME} PRIVATE ${GLEW_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${GLEW_LIBRARIES})
        endif()
    elseif(PC_GLEW_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW)
        target_include_directories(${PROJECT_NAME} PRIVATE ${PC_GLEW_INCLUDE_DIRS})
        target_link_directories(${PROJECT_NAME} PRIVATE ${PC_GLEW_LIBRARY_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${PC_GLEW_LINK_LIBRARIES})
    else()
        message(FATAL_ERROR "Linux/Unix: GLEW not found. Install e.g. sudo apt install libglew-dev")
    endif()
endif()
```

*`GAM200_JSLC/Engine/Engine.cpp` (약 116–122행)*

```cpp
#ifdef USE_GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLEW");
        return false;
    }
#endif
```

### 1.2 경고 관리 — Apple OpenGL 사용 중단(Deprecation) 경고 억제

macOS + 비-MSVC 툴체인에서 `GL_SILENCE_DEPRECATION`을 정의해 OpenGL 관련 deprecation 경고로 빌드 로그가 지저분해지는 것을 줄입니다.

*`GAM200_JSLC/CMakeLists.txt` (약 68–83행)*

```cmake
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE USE_GLEW)
else()
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
```

### 1.3 라이브러리 경로 자동화 — FMOD `dylib`를 실행 파일 옆에 두고 RPATH 설정

- `libfmod.dylib`를 빌드 산출물 디렉터리로 복사.
- `BUILD_RPATH "@executable_path"`로 **실행 파일과 같은 폴더**에서 동적 라이브러리를 찾도록 설정.

*`GAM200_JSLC/CMakeLists.txt` (약 227–243행)*

```cmake
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
```

---

## 2. Apple 전용 그래픽 파이프라인 (OpenGL 4.1)

### 2.1 코어 프로파일 + Forward Compatible

Apple이 지원하는 상한에 맞춰 **4.1 코어**, **Forward Compatible**을 요청합니다. (비 macOS는 3.3 코어.)

*`GAM200_JSLC/Engine/Engine.cpp` (약 66–75행)*

```cpp
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
```

### 2.2 Retina / 가상 해상도(1920×1080) vs 물리 픽셀

- **논리(가상) 해상도**: `Engine`의 `VIRTUAL_WIDTH` / `VIRTUAL_HEIGHT` (1920×1080). 씬 FBO는 이 크기로 렌더링합니다.
- **물리 픽셀**: `glfwGetFramebufferSize` 및 (macOS에서 전환 직후 불일치 시) `창 크기 × content scale`로 보정한 값을 `PostProcessManager`에 반영하고, `glViewport`와 포스트 패스 유니폼 `uFramebufferSize`에 사용합니다.

*`GAM200_JSLC/Engine/Engine.hpp` (약 18–19행)*

```cpp
constexpr int VIRTUAL_WIDTH = 1920;
constexpr int VIRTUAL_HEIGHT = 1080;
```

*`GAM200_JSLC/Engine/Engine.cpp` (약 136–142행)*

```cpp
    m_postProcess->Initialize(m_width, m_height);
    m_postProcess->SetPresentationWindow(m_window);

    // On HiDPI/Retina displays (e.g. macOS), the framebuffer can be larger than the window size.
    // Fetch the real framebuffer dimensions and inform PostProcessManager.
    SyncPostProcessDisplaySize();
```

*`GAM200_JSLC/OpenGL/PostProcessManager.cpp` (약 16–39행)*

```cpp
// After macOS Space / fullscreen transitions, glfwGetFramebufferSize can lag the real backing store
// for a frame while window size × content scale already matches Cocoa. That mismatch skews post.frag
// letterboxing (uFramebufferSize vs actual viewport pixels).
void QueryPresentationFramebufferSize(GLFWwindow* window, int& outW, int& outH)
{
    glfwGetFramebufferSize(window, &outW, &outH);
#ifdef __APPLE__
    int ww = 0, wh = 0;
    glfwGetWindowSize(window, &ww, &wh);
    float sx = 1.f, sy = 1.f;
    glfwGetWindowContentScale(window, &sx, &sy);
    const int sw = static_cast<int>(std::ceil(static_cast<float>(ww) * sx));
    const int sh = static_cast<int>(std::ceil(static_cast<float>(wh) * sy));
    if (sw <= 0 || sh <= 0)
        return;
    const int dw = outW > sw ? outW - sw : sw - outW;
    const int dh = outH > sh ? outH - sh : sh - outH;
    if (dw > 1 || dh > 1)
    {
        outW = sw;
        outH = sh;
    }
#endif
}
```

*`GAM200_JSLC/OpenGL/PostProcessManager.cpp` — `ApplyAndPresent()` 발췌 (약 169–204행)*

```cpp
void PostProcessManager::ApplyAndPresent()
{
    GL::Disable(GL_DEPTH_TEST);

    int fbW = 0;
    int fbH = 0;
    if (m_presentationWindow)
    {
#ifdef __APPLE__
        glfwPollEvents();
#endif
        QueryPresentationFramebufferSize(m_presentationWindow, fbW, fbH);
    }
    if (fbW <= 0 || fbH <= 0)
    {
        fbW = m_displayWidth;
        fbH = m_displayHeight;
    }
    if (fbW <= 0 || fbH <= 0)
        return;

    m_displayWidth = fbW;
    m_displayHeight = fbH;

    GL::Viewport(0, 0, fbW, fbH);
    GL::ClearColor(0.f, 0.f, 0.f, 1.f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    m_postShader->use();
    m_postShader->setVec2("uGameSize", static_cast<float>(m_width), static_cast<float>(m_height));
    m_postShader->setVec2("uFramebufferSize", static_cast<float>(fbW), static_cast<float>(fbH));
    // 이후 쿼드 드로우
}
```

포스트 프래그먼트 셰이더에서 **화면 종횡비**와 **게임(가상) 종횡비**를 비교해 레터박스/필러박스 UV를 계산합니다.

*`GAM200_JSLC/OpenGL/Shaders/post.frag` (약 35–57행)*

```glsl
void main()
{
    vec2 uv = vUV;
    float screenAspect = uFramebufferSize.x / max(uFramebufferSize.y, 1.0);
    float gameAspect = uGameSize.x / max(uGameSize.y, 1.0);

    const float kAspectEps = 1.0e-4;
    float aspectDiff = screenAspect - gameAspect;

    vec2 sceneUV = uv;
    if (aspectDiff > kAspectEps)
    {
        float w = gameAspect / screenAspect;
        float left = (1.0 - w) * 0.5;
        sceneUV.x = (uv.x - left) / w;
    }
    else if (aspectDiff < -kAspectEps)
    {
        float h = screenAspect / gameAspect;
        float bottom = (1.0 - h) * 0.5;
        sceneUV.y = (uv.y - bottom) / h;
    }
```

---

## 3. macOS 특화 이벤트 및 렌더링 보정

### 3.1 프레임 레이스(타이밍) 방지 — 렌더 직전 `glfwPollEvents` 추가

**프레임 레이스**: 창/모니터 상태가 바뀐 직후, 이벤트가 한 번에 다 처리되기 전에 프레임버퍼 크기를 읽으면 **이전 drawable 크기**가 남아 레터박스·비율이 틀어질 수 있습니다.  
`Update()` 이후 macOS에서만 `glfwPollEvents()`를 한 번 더 호출해, 전환 직후 커밋된 크기를 읽기 쉽게 합니다.

*`GAM200_JSLC/Engine/Engine.cpp` (약 177–187행)*

```cpp
#ifdef __APPLE__
        // Monitor/fullscreen transitions often commit after the first poll; reading FB size
        // immediately after Update() can still see the previous drawable (wrong letterbox).
        glfwPollEvents();
        // AppKit can restore the arrow cursor after Cmd+Tab even when focus callbacks ran.
        if (glfwGetWindowAttrib(m_window, GLFW_FOCUSED) &&
            glfwGetInputMode(m_window, GLFW_CURSOR) != GLFW_CURSOR_HIDDEN)
        {
            ApplyCustomCursorHidden();
        }
#endif
```

(같은 맥락으로 `ApplyAndPresent` 안에서도 macOS일 때 `glfwPollEvents()`를 호출합니다 — 위 2.2절 `PostProcessManager.cpp` 인용.)

### 3.2 사용자 편의 — 창 중앙 배치

macOS는 창을 자동으로 화면 중앙에 두지 않는 경우가 있어, 기본 모니터 해상도 기준으로 위치를 잡습니다.

*`GAM200_JSLC/Engine/Engine.cpp` (약 85–93행)*

```cpp
    // Center window on primary monitor (needed for macOS which doesn't auto-center)
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidmode = glfwGetVideoMode(primaryMonitor);
    if (vidmode)
    {
        int xpos = (vidmode->width  - m_width)  / 2;
        int ypos = (vidmode->height - m_height) / 2;
        glfwSetWindowPos(m_window, xpos, ypos);
    }
```

### 3.3 사용자 편의 — 포커스/커서 진입 시 커서 숨김 재적용

macOS에서 Cmd+Tab 등으로 포커스가 돌아오면 시스템이 커서를 다시 보이게 하는 경우가 있어, **포커스 획득 시** 및 **커서가 창 안으로 들어올 때** 다시 `GLFW_CURSOR_HIDDEN`을 적용합니다.

*`GAM200_JSLC/Engine/Engine.cpp` (약 102–105행)*

```cpp
    glfwSetKeyCallback(m_window, Engine::KeyCallback);
    glfwSetFramebufferSizeCallback(m_window, Engine::FramebufferSizeCallback);
    glfwSetWindowFocusCallback(m_window, Engine::WindowFocusCallback);
    glfwSetCursorEnterCallback(m_window, Engine::CursorEnterCallback);
```

*`GAM200_JSLC/Engine/Engine.cpp` (약 337–352행)*

```cpp
void Engine::WindowFocusCallback(GLFWwindow* window, int focused)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine && focused)
    {
        // macOS / WSL(X11) may re-show the system cursor after focus changes; re-apply hide.
        engine->ApplyCustomCursorHidden();
    }
}

void Engine::CursorEnterCallback(GLFWwindow* window, int entered)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine && entered)
        engine->ApplyCustomCursorHidden();
}
```

---

## 발표 한 줄 정리

**GLEW 없이 macOS 시스템 OpenGL을 쓰고, Retina용 프레임버퍼 크기 동기화·포스트 레터박스·전환 직후 `PollEvents`·창 중앙·커서 재숨김으로 Windows와 같은 1080p 논리 해상도 플레이를 맞춘다.**

---

