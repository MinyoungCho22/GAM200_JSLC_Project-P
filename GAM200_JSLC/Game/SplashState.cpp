#include "SplashState.hpp"
#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include <glad/glad.h>
#include <stb_image.h>

SplashState::SplashState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void SplashState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SplashState Initialize");
    shader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    shader->use();
    shader->setInt("imageTexture", 0);

    float vertices[] = { -0.5f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 1.0f, 0.0f, -0.5f, 0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("Asset/DigiPen.png", &width, &height, &nrChannels, 0);
    if (data)
    {
        imageWidth = width;
        imageHeight = height;
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else { Logger::Instance().Log(Logger::Severity::Error, "Failed to load texture: Asset/DigiPen.png"); }
    stbi_image_free(data);
}

void SplashState::Update(double dt)
{
    timer -= dt;
    if (timer <= 0.0)
    {
        gsm.ChangeState(std::make_unique<GameplayState>(gsm));
    }
}

void SplashState::Draw()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // 배경색 설정

    shader->use();

    Engine& engine = gsm.GetEngine();
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);
    Math::Vec2 imageSize = { static_cast<float>(imageWidth), static_cast<float>(imageHeight) };
    Math::Vec2 screenCenter = { engine.GetWidth() / 2.0f, engine.GetHeight() / 2.0f };
    Math::Matrix model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(imageSize);

    shader->setMat4("model", model);
    shader->setMat4("projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SplashState::Shutdown()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
    Logger::Instance().Log(Logger::Severity::Info, "SplashState Shutdown");
}