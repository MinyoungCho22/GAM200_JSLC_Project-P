#include "Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <fstream>
#include <sstream>
#include <vector>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
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

    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    vertex = GL::CreateShader(GL_VERTEX_SHADER);
    GL::ShaderSource(vertex, 1, &vShaderCode, NULL);
    GL::CompileShader(vertex);
    GL::GetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GL::GetShaderInfoLog(vertex, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: VERTEX COMPILATION_FAILED\n%s", infoLog);
    };

    fragment = GL::CreateShader(GL_FRAGMENT_SHADER);
    GL::ShaderSource(fragment, 1, &fShaderCode, NULL);
    GL::CompileShader(fragment);
    GL::GetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GL::GetShaderInfoLog(fragment, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: FRAGMENT COMPILATION_FAILED\n%s", infoLog);
    }

    ID = GL::CreateProgram();
    GL::AttachShader(ID, vertex);
    GL::AttachShader(ID, fragment);
    GL::LinkProgram(ID);
    GL::GetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        GL::GetProgramInfoLog(ID, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: PROGRAM LINKING_FAILED\n%s", infoLog);
    }

    GL::DeleteShader(vertex);
    GL::DeleteShader(fragment);
}

Shader::~Shader()
{
    GL::DeleteProgram(ID);
}

void Shader::use() const
{
    GL::UseProgram(ID);
}

void Shader::setInt(const std::string& name, int value) const
{
    GL::Uniform1i(GL::GetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, float v1, float v2, float v3) const
{
    GL::Uniform3f(GL::GetUniformLocation(ID, name.c_str()), v1, v2, v3);
}

void Shader::setMat4(const std::string& name, const Math::Matrix& mat) const
{
    GL::UniformMatrix4fv(GL::GetUniformLocation(ID, name.c_str()), 1, GL_FALSE, mat.Ptr());
}