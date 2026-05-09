//MainMenu.hpp

#pragma once
#include "../Engine/GameState.hpp"
#include <memory>

class GameStateManager;
class Shader;

class MainMenu : public GameState
{
public:
    MainMenu(GameStateManager& gsm);
    void Initialize() override;
    void Update(double dt) override;
    void Draw() override;
    void Shutdown() override;

private:
    GameStateManager& gsm;

    // Texture shader (simple.vert / simple.frag) – pos+UV quad
    std::unique_ptr<Shader> m_texShader;
    unsigned int m_texVAO = 0;
    unsigned int m_texVBO = 0;

    // Background: Asset/MainMenu.png
    unsigned int m_bgTexID = 0;
    int          m_bgTexW  = 0;
    int          m_bgTexH  = 0;

    // Selection indicator: Asset/MainMenu_Click.png
    unsigned int m_clickTexID = 0;
    int          m_clickTexW  = 0;
    int          m_clickTexH  = 0;

    // Menu navigation: 0=Play  1=Settings  2=Credits  3=Exit
    int  m_selectedItem        = 0;
    bool m_waitForEnterRelease = false;
    bool m_waitForNavRelease   = false;

    // Fade-out overlay
    std::unique_ptr<Shader> m_fadeShader;
    unsigned int m_fadeVAO    = 0;
    unsigned int m_fadeVBO    = 0;
    bool  m_isFadingOut       = false;
    float m_fadeAlpha         = 0.0f;
    int   m_pendingAction     = -1;

};
