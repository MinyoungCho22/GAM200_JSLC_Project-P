#include "Drone.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <iostream>

#include <stb_image.h>

void Drone::Init(Math::Vec2 startPos, const char* texturePath)
{
    drone_position = startPos;
    drone_velocity = { 0.0f, 0.0f };
    drone_direction = { 1.0f, 0.0f };

    float vertices[] = {
        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,

        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 0.0f
    };

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        float desiredWidth = 100.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        drone_size = { desiredWidth, desiredWidth * aspectRatio };
    }
    else
    {
        std::cout << "Failed to load drone texture: " << texturePath << std::endl;
    }
    stbi_image_free(data);
}

void Drone::Update(double dt)
{
    move_timer += static_cast<float>(dt);

    if (move_timer > 3.0f)
    {
        move_timer = 0.0f;
        drone_direction.x = -drone_direction.x;
    }

    drone_velocity = drone_direction * drone_speed;
    drone_position += drone_velocity * static_cast<float>(dt);
}

void Drone::Draw(const Shader& shader) const
{
    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(drone_size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ drone_position.x, drone_position.y + drone_velocity.y / 2.0f });
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.setMat4("model", model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Drone::Shutdown()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
}
