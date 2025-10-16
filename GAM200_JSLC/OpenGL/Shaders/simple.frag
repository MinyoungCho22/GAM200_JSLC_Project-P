#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

// [����] C++ �ڵ�� uniform ���� �̸��� ���� (ourTexture -> imageTexture)
uniform sampler2D imageTexture;
// [�߰�] C++���� ���޹��� ���� ��
uniform float u_alpha;

void main()
{
    // �ؽ�ó���� ������ �����ɴϴ�.
    vec4 texColor = texture(imageTexture, TexCoord);
    
    // C++���� ���� u_alpha ���� ���� ������ �����ݴϴ�.
    // (u_alpha�� 1.0�̸� ���� ������, 0.0�̸� ���� ����)
    FragColor = vec4(texColor.rgb, texColor.a * u_alpha);
}