#include "SplashState.hpp"
#include "GameplayState.hpp"
#include "../Engine/GameStateManager.hpp"
#include "../Engine/Engine.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp" 

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

SplashState::SplashState(GameStateManager& gsm_ref) : gsm(gsm_ref) {}

void SplashState::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "SplashState Initialize");
    shader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    shader->use();

    shader->setInt("ourTexture", 0);

    float vertices[] = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,

        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);

    GL::GenTextures(1, &textureID);
    GL::BindTexture(GL_TEXTURE_2D, textureID);

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("Asset/DigiPen.png", &width, &height, &nrChannels, 0);
    if (data)
    {
        imageWidth = width;
        imageHeight = height;
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);
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
    GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    shader->use();

    Engine& engine = gsm.GetEngine();
    // ✅ projection은 (0, 1920, 0, 1080) 기준으로 생성됨
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(engine.GetWidth()), 0.0f, static_cast<float>(engine.GetHeight()), -1.0f, 1.0f);

    // ✅ [수정] 이미지 원본 크기를 그대로 사용합니다.
    Math::Vec2 imageSize = { (float)imageWidth, (float)imageHeight };
    // ✅ [수정] 가상 스크린(1920x1080)의 중앙에 배치합니다.
    Math::Vec2 screenCenter = { engine.GetWidth() / 2.0f, engine.GetHeight() / 2.0f };

    Math::Matrix model = Math::Matrix::CreateTranslation(screenCenter) * Math::Matrix::CreateScale(imageSize);

    shader->setMat4("model", model);
    shader->setMat4("projection", projection);

    shader->setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader->setBool("flipX", false);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void SplashState::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &textureID);
    Logger::Instance().Log(Logger::Severity::Info, "SplashState Shutdown");
}