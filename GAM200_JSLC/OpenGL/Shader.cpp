#include "Shader.hpp"
#include "../Engine/Logger.hpp" // Logger ���
#include "../Engine/Matrix.hpp" // Math::Matrix ���
#include <fstream>
#include <sstream>
#include <vector>
#include <glad/glad.h>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // 1. ���Ͽ��� ���ؽ�/�����׸�Ʈ ���̴� �ڵ� �˻�
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        vShaderFile.close();
        fShaderFile.close();
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: FILE_NOT_SUCCESFULLY_READ: %s", e.what());
    }
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // 2. ���̴� ������ �� ���� Ȯ��
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: VERTEX COMPILATION_FAILED\n%s", infoLog);
    };

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: FRAGMENT COMPILATION_FAILED\n%s", infoLog);
    }

    // 3. ���̴� ���α׷� ��ũ �� ���� Ȯ��
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: PROGRAM LINKING_FAILED\n%s", infoLog);
    }

    // ��ũ �Ŀ��� �� �̻� �ʿ� �����Ƿ� ���̴����� �����մϴ�.
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader()
{
    glDeleteProgram(ID);
}

void Shader::use() const
{
    glUseProgram(ID);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, float v1, float v2, float v3) const
{
    glUniform3f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3);
}

// Shader.cpp �� setMat4 �Լ�
void Shader::setMat4(const std::string& name, const Math::Matrix& mat) const
{
    // [����] GL_TRUE -> GL_FALSE �� ����
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, mat.Ptr());
}