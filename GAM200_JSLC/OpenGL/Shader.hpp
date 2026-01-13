//Shader.hpp

#pragma once
#include <string>

// Forward declaration for the Matrix class within the Math namespace
namespace Math { class Matrix; }

class Shader
{
public:
    // Constructor: Reads and builds the shader from vertex and fragment source files
    Shader(const char* vertexPath, const char* fragmentPath);
    
    // Destructor: Cleans up shader programs from the GPU
    ~Shader();

    // Activate the shader program for use
    void use() const;

    // --- Uniform setter functions ---
    
    // Set an integer value (often used for texture units/samplers)
    void setInt(const std::string& name, int value) const;
    
    // Set a 3-component vector (often used for RGB colors or 3D positions)
    void setVec3(const std::string& name, float v1, float v2, float v3) const;
    
    // Set a 4x4 transformation matrix (used for Model-View-Projection)
    void setMat4(const std::string& name, const Math::Matrix& mat) const;

    // Set a 4-component vector (often used for RGBA colors or clipping planes)
    void setVec4(const std::string& name, float v1, float v2, float v3, float v4) const;
    
    // Set a single floating-point value
    void setFloat(const std::string& name, float value) const;
    
    // Set a boolean value (converted to int/float for the GPU)
    void setBool(const std::string& name, bool value) const;

private:
    // The program ID assigned by OpenGL
    unsigned int ID;
};