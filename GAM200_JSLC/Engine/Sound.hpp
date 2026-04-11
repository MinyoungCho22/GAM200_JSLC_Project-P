//Sound.hpp

#pragma once
#include <string>
#include <map>

#if defined(__EMSCRIPTEN__)
    // SoLoud: forward declarations only (pointers in private members)
    namespace SoLoud { class Soloud; class Wav; }
#elif defined(DISABLE_FMOD)
    namespace FMOD { class System; class Sound; class Channel; }
#else
    #include "fmod.hpp"
#endif

class SoundSystem
{
public:
    static SoundSystem& Instance();
    bool Initialize();
    void Update();
    void Shutdown();

    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_masterVolume; }

#if defined(__EMSCRIPTEN__)
    SoLoud::Soloud* GetSoLoud() const { return m_soloud; }
#else
    FMOD::System* GetSystem() const { return m_system; }
#endif

private:
    SoundSystem() = default;
    ~SoundSystem() = default;

#if defined(__EMSCRIPTEN__)
    SoLoud::Soloud* m_soloud = nullptr;
#else
    FMOD::System* m_system         = nullptr;
    void*         m_extraDriverData = nullptr;
#endif
    float m_masterVolume = 0.8f;
};

class Sound
{
public:
    Sound();
    ~Sound();

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    Sound(Sound&& other) noexcept;
    Sound& operator=(Sound&& other) noexcept;

    bool Load(const std::string& filepath, bool loop = false);

    void Play();
    void Stop();
    void SetPaused(bool paused);
    void SetVolume(float volume);
    void SetPitch(float pitch);
    bool IsPlaying() const;

private:
#if defined(__EMSCRIPTEN__)
    SoLoud::Wav* m_wav    = nullptr;
    unsigned int m_handle = 0;
    float        m_volume = 1.0f;
    float        m_pitch  = 1.0f;
#else
    FMOD::Sound*   m_sound   = nullptr;
    FMOD::Channel* m_channel = nullptr;
#endif
    bool m_isLoaded  = false;
    bool m_isLooping = false;
};
