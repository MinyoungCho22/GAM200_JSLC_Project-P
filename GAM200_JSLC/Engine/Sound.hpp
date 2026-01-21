//Sound.hpp

#pragma once
#include <string>
#include <map>

#ifdef DISABLE_FMOD
namespace FMOD
{
    class System;
    class Sound;
    class Channel;
}
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

    FMOD::System* GetSystem() const { return m_system; }

private:
    SoundSystem() = default;
    ~SoundSystem() = default;

    FMOD::System* m_system = nullptr;
    void* m_extraDriverData = nullptr;
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
    FMOD::Sound* m_sound = nullptr;
    FMOD::Channel* m_channel = nullptr;
    bool m_isLoaded = false;
    bool m_isLooping = false;
};