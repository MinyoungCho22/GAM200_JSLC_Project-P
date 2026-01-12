//Background.cpp
#include "Background.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/QuadMesh.hpp"
#include <iostream>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

void Background::Initialize(const char* texturePath)
{
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (!data)
    {
        std::cout << "Failed to load background texture: " << texturePath << std::endl;
        return;
    }

    GL::GenTextures(1, &m_textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    GL::GenerateMipmap(GL_TEXTURE_2D);

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

void Background::Shutdown()
{
    GL::DeleteTextures(1, &m_textureID);
}

void Background::Draw(Shader& shader, const Math::Matrix& model)
{
    shader.setMat4("model", model);

    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", false);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    OpenGL::QuadMesh::Bind();
    OpenGL::QuadMesh::Draw();
    OpenGL::QuadMesh::Unbind();
}