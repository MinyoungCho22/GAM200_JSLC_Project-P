#pragma once
#include <string>

// ���� ����: Matrix.hpp�� ���� �������� �ʾƵ� �ǰ� �Ͽ� ������ �ӵ��� ���Դϴ�.
namespace Math { class Matrix; }

class Shader
{
public:
    // ���̴� ���α׷��� ���� ID (OpenGL�� �ο�)
    unsigned int ID;

    // ������: ���ؽ�/�����׸�Ʈ ���̴� ���� ��θ� �޾� ���α׷��� �����մϴ�.
    Shader(const char* vertexPath, const char* fragmentPath);
    // �Ҹ���: ���α׷��� �Ҹ�� �� GPU �޸𸮿��� ���� ���ҽ��� �����մϴ�.
    ~Shader();

    // �� ���̴� ���α׷��� ���� ������ ���������ο��� ����ϵ��� Ȱ��ȭ�մϴ�.
    void use() const;

    // Uniform ���� ���� �Լ���
    void setInt(const std::string& name, int value) const;   // [�߰�] ���� (�ؽ�ó ���� ��) uniform ����
    void setVec3(const std::string& name, float v1, float v2, float v3) const;
    void setMat4(const std::string& name, const Math::Matrix& mat) const;
};