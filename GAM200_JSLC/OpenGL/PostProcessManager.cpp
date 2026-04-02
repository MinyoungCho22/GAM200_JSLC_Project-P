// PostProcessManager.cpp

#include "PostProcessManager.h"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Logger.hpp"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

// ---------------------------
// Internal helpers (optional)
// ---------------------------
static void DestroySceneTargets(GLuint& fbo, GLuint& colorTex, GLuint& depthRbo)
{
    if (depthRbo)
    {
        GL::DeleteRenderbuffers(1, &depthRbo);
        depthRbo = 0;
    }
    if (colorTex)
    {
        GL::DeleteTextures(1, &colorTex);
        colorTex = 0;
    }
    if (fbo)
    {
        GL::DeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
}

void PostProcessManager::Initialize(int width, int height)
{
    m_width = width;
    m_height = height;
    m_displayWidth = width;
    m_displayHeight = height;

    CreateFullscreenQuad();
    CreateSceneFBO();

    m_postShader = std::make_unique<Shader>(
        "OpenGL/Shaders/post.vert",
        "OpenGL/Shaders/post.frag"
    );

    m_postShader->use();
    m_postShader->setInt("uSceneTex", 0);
    m_postShader->setInt("uLightOverlayTex", 1);
    m_postShader->setFloat("uExposure", m_settings.exposure);
    m_postShader->setBool("uUseLightOverlay", m_settings.useLightOverlay);
    m_postShader->setFloat("uLightOverlayStrength", m_settings.lightOverlayStrength);
    m_postShader->setVec2("uFramebufferSize", static_cast<float>(m_width), static_cast<float>(m_height));

    m_lightOverlay = std::make_unique<Background>();
    m_lightOverlay->Initialize("Asset/Hallway_Light.png");
}

void PostProcessManager::Shutdown()
{
    m_presentationWindow = nullptr;

    if (m_lightOverlay)
    {
        m_lightOverlay->Shutdown();
        m_lightOverlay.reset();
    }

    if (m_postShader)
        m_postShader.reset();

    if (m_quadVAO)
    {
        GL::DeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO)
    {
        GL::DeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }

    DestroySceneTargets(m_sceneFBO, m_sceneColorTex, m_sceneDepthRBO);

    m_width = 0;
    m_height = 0;
}

void PostProcessManager::Resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    DestroySceneTargets(m_sceneFBO, m_sceneColorTex, m_sceneDepthRBO);
    CreateSceneFBO();
}

void PostProcessManager::BeginScene()
{
    GL::BindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    GL::Viewport(0, 0, m_width, m_height);

    GL::Disable(GL_DEPTH_TEST);

    GL::ClearColor(0.f, 0.f, 0.f, 1.f);
    GL::Clear(GL_COLOR_BUFFER_BIT); 
}


void PostProcessManager::EndScene()
{
    GL::BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessManager::GetLetterboxViewport(int& outX, int& outY, int& outW, int& outH) const
{
    ComputeLetterboxViewport(m_displayWidth, m_displayHeight, outX, outY, outW, outH);
}

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
        glfwGetFramebufferSize(m_presentationWindow, &fbW, &fbH);
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
    m_postShader->setFloat("uExposure", m_passthrough ? 1.0f : m_settings.exposure);
    m_postShader->setBool("uUseLightOverlay", !m_passthrough && m_settings.useLightOverlay);
    m_postShader->setFloat("uLightOverlayStrength", m_settings.lightOverlayStrength);

    m_postShader->setVec2("uCameraPos", m_settings.cameraPos.x, m_settings.cameraPos.y);
    m_postShader->setVec2("uGameSize", static_cast<float>(m_width), static_cast<float>(m_height));
    m_postShader->setVec2("uFramebufferSize", static_cast<float>(fbW), static_cast<float>(fbH));

    m_postShader->setVec2("uHallwayMin", 1920.0f, 0.0f);
    m_postShader->setVec2("uHallwaySize", 5940.0f, 1080.0f);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_sceneColorTex);

    GL::ActiveTexture(GL_TEXTURE1);
    if (m_lightOverlay)
        GL::BindTexture(GL_TEXTURE_2D, m_lightOverlay->GetTextureID());
    else
        GL::BindTexture(GL_TEXTURE_2D, 0);

    GL::BindVertexArray(m_quadVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);

    GL::ActiveTexture(GL_TEXTURE1);
    GL::BindTexture(GL_TEXTURE_2D, 0);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, 0);
}

void PostProcessManager::ComputeLetterboxViewport(int dispW, int dispH, int& outX, int& outY, int& outW, int& outH) const
{
    outX = 0;
    outY = 0;
    outW = dispW;
    outH = dispH;

    if (dispW <= 0 || dispH <= 0 || m_width <= 0 || m_height <= 0) return;

    const float virtualAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    const float displayAspect = static_cast<float>(dispW) / static_cast<float>(dispH);

    if (displayAspect > virtualAspect)
    {
        outH = dispH;
        outW = static_cast<int>(static_cast<float>(outH) * virtualAspect);
        outX = (dispW - outW) / 2;
        outY = 0;
    }
    else if (displayAspect < virtualAspect)
    {
        outW = dispW;
        outH = static_cast<int>(static_cast<float>(outW) / virtualAspect);
        outX = 0;
        outY = (dispH - outH) / 2;
    }
}

void PostProcessManager::CreateSceneFBO()
{
    DestroySceneTargets(m_sceneFBO, m_sceneColorTex, m_sceneDepthRBO);

    GL::GenFramebuffers(1, &m_sceneFBO);
    GL::BindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);

    // ------------------------
    // Color texture attachment
    // ------------------------
    GL::GenTextures(1, &m_sceneColorTex);
    GL::BindTexture(GL_TEXTURE_2D, m_sceneColorTex);

    GL::TexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        m_width,
        m_height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneColorTex, 0);

    // -------------------------
    // Depth+Stencil renderbuffer
    // -------------------------
    GL::GenRenderbuffers(1, &m_sceneDepthRBO);
    GL::BindRenderbuffer(GL_RENDERBUFFER, m_sceneDepthRBO);
    GL::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    GL::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_sceneDepthRBO);

    // ------------------------
    // FBO status check
    // ------------------------
    GLenum status = GL::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        Logger::Instance().Log(
            Logger::Severity::Error,
            "PostProcess FBO incomplete (status=%u) size=%dx%d",
            status,
            m_width,
            m_height
        );
    }

    // Unbind
    GL::BindFramebuffer(GL_FRAMEBUFFER, 0);
    GL::BindTexture(GL_TEXTURE_2D, 0);
    GL::BindRenderbuffer(GL_RENDERBUFFER, 0);
}

void PostProcessManager::CreateFullscreenQuad()
{
    float vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,

        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    GL::GenVertexArrays(1, &m_quadVAO);
    GL::GenBuffers(1, &m_quadVBO);

    GL::BindVertexArray(m_quadVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);

    GL::BindVertexArray(0);
}
