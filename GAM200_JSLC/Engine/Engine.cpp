#include "Engine.hpp"
#include "Logger.hpp"
#include "../OpenGL/Shader.hpp"
#include "Matrix.hpp"
#include "Vec2.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <stb_image.h>

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Initialize(const std::string& windowTitle)
{
    Logger::Instance().Log(Logger::Severity::Event, "Engine Start");

    if (!glfwInit())
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (!primaryMonitor)
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to get primary monitor");
        glfwTerminate();
        return false;
    }
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    if (!mode)
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to get video mode for monitor");
        glfwTerminate();
        return false;
    }
    m_width = mode->width;
    m_height = mode->height;
    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), primaryMonitor, nullptr);

    if (!m_window)
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    Logger::Instance().Log(Logger::Severity::Event, "Window created successfully");

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLAD");
        return false;
    }
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- 스플래시 스크린 리소스 초기화 ---
    Logger::Instance().Log(Logger::Severity::Info, "Loading splash screen assets...");
    float splash_vertices[] = { -0.5f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 1.0f, 0.0f, -0.5f, 0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f };
    glGenVertexArrays(1, &m_splashVAO);
    glGenBuffers(1, &m_splashVBO);
    glBindVertexArray(m_splashVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_splashVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(splash_vertices), splash_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &m_splashTextureID);
    glBindTexture(GL_TEXTURE_2D, m_splashTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("Asset/DigiPen.png", &width, &height, &nrChannels, 0);
    if (data)
    {
        m_splashImageWidth = width;
        m_splashImageHeight = height;
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else { Logger::Instance().Log(Logger::Severity::Error, "Failed to load texture: Asset/DigiPen.png"); }
    stbi_image_free(data);

    // --- 게임플레이 리소스 초기화 ---
    m_shader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_solid_color_shader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");
    m_shader->use();
    m_shader->setInt("imageTexture", 0);
    Logger::Instance().Log(Logger::Severity::Info, "Shaders loaded");

    float line_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
    glGenVertexArrays(1, &m_groundVAO);
    glGenBuffers(1, &m_groundVBO);
    glBindVertexArray(m_groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const char* playerTexturePath = "Asset/player.png";
    m_player.Init({ 100.0f, 300.0f }, playerTexturePath);
    Logger::Instance().Log(Logger::Severity::Info, "Player initialized with texture: %s", playerTexturePath);
    Logger::Instance().Log(Logger::Severity::Event, "Engine initialization successful");

    return true;
}

void Engine::Update()
{
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_window, true);
    }

    switch (m_currentState)
    {
    case State::Splash:
    {
        m_splashTimer -= m_deltaTime;
        if (m_splashTimer <= 0.0)
        {
            m_currentState = State::Gameplay;
            Logger::Instance().Log(Logger::Severity::Event, "Transitioning to Gameplay state");
        }
        break;
    }
    case State::Gameplay:
    {
        if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) m_player.MoveLeft();
        if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) m_player.MoveRight();
        if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) m_player.Jump();
        if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) m_player.Crouch();
        else m_player.StopCrouch();
        if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) m_player.Dash();
        m_player.Update(m_deltaTime);
        break;
    }
    }
}

void Engine::Draw() const
{
    switch (m_currentState)
    {
    case State::Splash:
        DrawSplashScreen();
        break;
    case State::Gameplay:
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(m_width), 0.0f, static_cast<float>(m_height), -1.0f, 1.0f);

        m_shader->use();
        m_shader->setMat4("projection", projection);
        m_player.Draw(*m_shader);

        m_solid_color_shader->use();
        m_solid_color_shader->setMat4("projection", projection);
        Math::Vec2 groundPosition = { m_width / 2.0f, 100.0f };
        Math::Vec2 groundSize = { static_cast<float>(m_width), 2.0f };
        Math::Matrix groundModel = Math::Matrix::CreateTranslation(groundPosition) * Math::Matrix::CreateScale(groundSize);
        m_solid_color_shader->setMat4("model", groundModel);
        m_solid_color_shader->setVec3("objectColor", 0.8f, 0.8f, 0.8f);
        glBindVertexArray(m_groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        break;
    }
    }
}

void Engine::DrawSplashScreen() const
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader->use();

    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(m_width), 0.0f, static_cast<float>(m_height), -1.0f, 1.0f);

    Math::Vec2 imageSize = { static_cast<float>(m_splashImageWidth), static_cast<float>(m_splashImageHeight) };
    Math::Vec2 screenCenter = { m_width / 2.0f, m_height / 2.0f };
    Math::Matrix model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(imageSize);

    m_shader->setMat4("model", model);
    m_shader->setMat4("projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_splashTextureID);

    glBindVertexArray(m_splashVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Engine::Shutdown()
{
    m_player.Shutdown();
    glDeleteVertexArrays(1, &m_groundVAO);
    glDeleteBuffers(1, &m_groundVBO);

    glDeleteVertexArrays(1, &m_splashVAO);
    glDeleteBuffers(1, &m_splashVBO);
    glDeleteTextures(1, &m_splashTextureID);

    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    Logger::Instance().Log(Logger::Severity::Event, "Engine Stopped");
}

void Engine::GameLoop()
{
    m_lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(m_window))
    {
        double currentFrameTime = glfwGetTime();
        m_deltaTime = currentFrameTime - m_lastFrameTime;
        m_lastFrameTime = currentFrameTime;

        glfwPollEvents();
        Update();
        Draw();
        glfwSwapBuffers(m_window);
    }
}