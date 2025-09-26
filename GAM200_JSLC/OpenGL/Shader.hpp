#pragma once
#include <string>

// 전방 선언: Matrix.hpp를 직접 포함하지 않아도 되게 하여 컴파일 속도를 높입니다.
namespace Math { class Matrix; }

class Shader
{
public:
    // 셰이더 프로그램의 고유 ID (OpenGL이 부여)
    unsigned int ID;

    // 생성자: 버텍스/프래그먼트 셰이더 파일 경로를 받아 프로그램을 생성합니다.
    Shader(const char* vertexPath, const char* fragmentPath);
    // 소멸자: 프로그램이 소멸될 때 GPU 메모리에서 관련 리소스를 해제합니다.
    ~Shader();

    // 이 셰이더 프로그램을 현재 렌더링 파이프라인에서 사용하도록 활성화합니다.
    void use() const;

    // Uniform 변수 설정 함수들
    void setInt(const std::string& name, int value) const;   // [추가] 정수 (텍스처 유닛 등) uniform 설정
    void setVec3(const std::string& name, float v1, float v2, float v3) const;
    void setMat4(const std::string& name, const Math::Matrix& mat) const;
};