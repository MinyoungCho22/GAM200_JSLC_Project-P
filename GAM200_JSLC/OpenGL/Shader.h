#pragma once
#include <string>
#include "../Engine/Matrix.h"

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use();
    void setMat4(const std::string& name, const Matrix& mat) const;
};