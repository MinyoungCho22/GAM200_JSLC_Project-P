#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

// [수정] C++ 코드와 uniform 변수 이름을 통일 (ourTexture -> imageTexture)
uniform sampler2D imageTexture;
// [추가] C++에서 전달받을 투명도 값
uniform float u_alpha;

void main()
{
    // 텍스처에서 색상을 가져옵니다.
    vec4 texColor = texture(imageTexture, TexCoord);
    
    // C++에서 받은 u_alpha 값을 최종 투명도에 곱해줍니다.
    // (u_alpha가 1.0이면 완전 불투명, 0.0이면 완전 투명)
    FragColor = vec4(texColor.rgb, texColor.a * u_alpha);
}