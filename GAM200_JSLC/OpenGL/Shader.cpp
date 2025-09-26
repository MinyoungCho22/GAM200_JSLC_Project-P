#include "Shader.hpp"
#include "../Engine/Logger.hpp" // Logger 사용
#include "../Engine/Matrix.hpp" // Math::Matrix 사용
#include <fstream>
#include <sstream>
#include <vector>
#include <glad/glad.h>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // 1. 파일에서 버텍스/프래그먼트 셰이더 코드 검색
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

    // 2. 셰이더 컴파일 및 오류 확인
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

    // 3. 셰이더 프로그램 링크 및 오류 확인
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

    // 링크 후에는 더 이상 필요 없으므로 셰이더들을 삭제합니다.
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

// Shader.cpp 의 setMat4 함수
void Shader::setMat4(const std::string& name, const Math::Matrix& mat) const
{
    // [수정] GL_TRUE -> GL_FALSE 로 변경
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, mat.Ptr());
}