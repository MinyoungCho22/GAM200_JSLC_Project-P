#pragma once

namespace CS200
{
    struct RGBA; // RGBA 구조체를 사용한다고 미리 알려줍니다 (전방 선언).

    class RenderingAPI
    {
    public:
        // 렌더링 API를 초기화합니다.
        static void Init();

        // 화면을 지울 색상을 설정합니다.
        static void SetClearColor(RGBA color);

        // 설정된 색상으로 화면을 지웁니다.
        static void Clear();
    };
}