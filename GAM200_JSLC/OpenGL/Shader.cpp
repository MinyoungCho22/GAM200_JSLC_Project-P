//Shader.cpp

#include "Shader.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <fstream>
#include <sstream>
#include <vector>

#ifdef __EMSCRIPTEN__
// WebGL2(GLSL ES 3.0)용 셰이더 소스 자동 변환:
// #version 330 core 줄을 제거하고, 파일 맨 앞에 #version 300 es 를 삽입한다.
// (셰이더 파일 첫 줄이 주석인 경우에도 #version이 1번 줄에 오도록 보장)
static std::string PatchShaderForWebGL(const std::string& src)
{
    std::string out = src;
    const std::string from = "#version 330 core";
    auto pos = out.find(from);
    if (pos != std::string::npos)
    {
        // 원본 #version 줄(+ 뒤따르는 개행)을 제거
        size_t endPos = pos + from.size();
        if (endPos < out.size() && out[endPos] == '\n')
            ++endPos;
        out.erase(pos, endPos - pos);
        // WebGL2 버전 지시자를 파일 맨 앞에 삽입
        out = "#version 300 es\nprecision highp float;\n" + out;
    }
    return out;
}
#endif

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // 1. Retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    // Ensure ifstream objects can throw exceptions
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try
    {
        // Open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        
        // Read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        
        // Close file handlers
        vShaderFile.close();
        fShaderFile.close();
        
        // Convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: FILE_NOT_SUCCESSFULLY_READ: %s", e.what());
    }

#ifdef __EMSCRIPTEN__
    vertexCode   = PatchShaderForWebGL(vertexCode);
    fragmentCode = PatchShaderForWebGL(fragmentCode);
#endif

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // 2. Compile shaders
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Vertex Shader
    vertex = GL::CreateShader(GL_VERTEX_SHADER);
    GL::ShaderSource(vertex, 1, &vShaderCode, NULL);
    GL::CompileShader(vertex);
    
    // Check for compilation errors
    GL::GetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GL::GetShaderInfoLog(vertex, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: VERTEX COMPILATION_FAILED\n%s", infoLog);
    };

    // Fragment Shader
    fragment = GL::CreateShader(GL_FRAGMENT_SHADER);
    GL::ShaderSource(fragment, 1, &fShaderCode, NULL);
    GL::CompileShader(fragment);
    
    // Check for compilation errors
    GL::GetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GL::GetShaderInfoLog(fragment, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: FRAGMENT COMPILATION_FAILED\n%s", infoLog);
    }

    // 3. Shader Program linking
    ID = GL::CreateProgram();
    GL::AttachShader(ID, vertex);
    GL::AttachShader(ID, fragment);
    GL::LinkProgram(ID);
    
    // Check for linking errors
    GL::GetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        GL::GetProgramInfoLog(ID, 512, NULL, infoLog);
        Logger::Instance().Log(Logger::Severity::Error, "SHADER: PROGRAM LINKING_FAILED\n%s", infoLog);
    }

    // Delete the shaders as they're linked into our program now and no longer necessary
    GL::DeleteShader(vertex);
    GL::DeleteShader(fragment);
}

Shader::~Shader()
{
    // Clean up the shader program from the GPU
    GL::DeleteProgram(ID);
}

void Shader::use() const
{
    // Tell OpenGL to use this specific shader program
    GL::UseProgram(ID);
}

// --- Uniform Utilities ---

void Shader::setInt(const std::string& name, int value) const
{
    GL::Uniform1i(GL::GetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, float v1, float v2) const
{
    GL::Uniform2f(GL::GetUniformLocation(ID, name.c_str()), v1, v2);
}

void Shader::setVec3(const std::string& name, float v1, float v2, float v3) const
{
    GL::Uniform3f(GL::GetUniformLocation(ID, name.c_str()), v1, v2, v3);
}

void Shader::setMat4(const std::string& name, const Math::Matrix& mat) const
{
    // Upload the 4x4 matrix data to the GPU
    GL::UniformMatrix4fv(GL::GetUniformLocation(ID, name.c_str()), 1, GL_FALSE, mat.Ptr());
}

void Shader::setVec4(const std::string& name, float v1, float v2, float v3, float v4) const
{
    GL::Uniform4f(GL::GetUniformLocation(ID, name.c_str()), v1, v2, v3, v4);
}

void Shader::setBool(const std::string& name, bool value) const
{
    // Booleans are passed to GLSL as integers (0 or 1)
    GL::Uniform1i(GL::GetUniformLocation(ID, name.c_str()), static_cast<int>(value));
}

void Shader::setFloat(const std::string& name, float value) const
{
    GL::Uniform1f(GL::GetUniformLocation(ID, name.c_str()), value);
}