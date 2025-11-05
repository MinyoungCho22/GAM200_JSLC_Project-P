#pragma once
#include <string>

namespace Math { class Matrix; }

class Shader
{
public:
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setVec3(const std::string& name, float v1, float v2, float v3) const;
    void setMat4(const std::string& name, const Math::Matrix& mat) const;

    
    void setVec4(const std::string& name, float v1, float v2, float v3, float v4) const;
    
    void setBool(const std::string& name, bool value) const;

private:
    unsigned int ID; // 셰이더 프로그램 ID
};