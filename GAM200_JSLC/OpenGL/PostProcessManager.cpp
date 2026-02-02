// PostProcessManager.cpp

#include "PostProcessManager.h"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Logger.hpp"

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

    CreateFullscreenQuad();
    CreateSceneFBO();

    m_postShader = std::make_unique<Shader>(
        "OpenGL/Shaders/post.vert",
        "OpenGL/Shaders/post.frag"
    );

    m_postShader->use();
    m_postShader->setInt("uSceneTex", 0);
    m_postShader->setFloat("uExposure", m_settings.exposure);
}

void PostProcessManager::Shutdown()
{
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

void PostProcessManager::ApplyAndPresent()
{
    GL::Disable(GL_DEPTH_TEST);

    GL::Viewport(0, 0, m_width, m_height);
    GL::ClearColor(0.f, 0.f, 0.f, 1.f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    m_postShader->use();
    m_postShader->setFloat("uExposure", m_settings.exposure);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_sceneColorTex);

    GL::BindVertexArray(m_quadVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);

    GL::BindTexture(GL_TEXTURE_2D, 0);
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
