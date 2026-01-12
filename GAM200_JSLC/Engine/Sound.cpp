//Sound.cpp

// Visual Studio에서는 pragma comment로 링크, CMake에서는 CMakeLists.txt에서 링크
#ifndef CMAKE_BUILD
#pragma comment(lib, "Engine/fmod_vc.lib")
#endif
#include "Logger.hpp"
#include "Sound.hpp"
#include <iostream>


SoundSystem& SoundSystem::Instance()
{
    static SoundSystem instance;
    return instance;
}

bool SoundSystem::Initialize()
{
    if (m_system != nullptr) return true;

    FMOD_RESULT result;

    result = FMOD::System_Create(&m_system);
    if (result != FMOD_OK)
    {
        std::cout << "FMOD System Create Failed!" << std::endl;
        return false;
    }

    result = m_system->init(512, FMOD_INIT_NORMAL, m_extraDriverData);
    if (result != FMOD_OK)
    {
        std::cout << "FMOD System Init Failed!" << std::endl;
        return false;
    }

    //Logger::Instance().Log(Logger::Severity::Info, "FMOD Audio System Initialized.");
    return true;
}

void SoundSystem::Update()
{
    if (m_system)
    {
        m_system->update();
    }
}

void SoundSystem::Shutdown()
{
    if (m_system)
    {
        m_system->close();
        m_system->release();
        m_system = nullptr;
    }
}

Sound::Sound() : m_sound(nullptr), m_channel(nullptr), m_isLoaded(false)
{
}

Sound::~Sound()
{
}

bool Sound::Load(const std::string& filepath, bool loop)
{
    if (!SoundSystem::Instance().GetSystem())
    {
        SoundSystem::Instance().Initialize();
    }

    FMOD::System* system = SoundSystem::Instance().GetSystem();
    FMOD_RESULT result;

    FMOD_MODE mode = FMOD_DEFAULT;
    mode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    m_isLooping = loop;

    result = system->createSound(filepath.c_str(), mode, 0, &m_sound);
    if (result != FMOD_OK)
    {
        std::cout << "Failed to load sound: " << filepath << " Error: " << result << std::endl;
        return false;
    }

    m_isLoaded = true;
    return true;
}

void Sound::Play()
{
    if (!m_isLoaded || !m_sound) return;

    FMOD::System* system = SoundSystem::Instance().GetSystem();

    bool isPlaying = false;
    if (m_channel)
    {
        m_channel->isPlaying(&isPlaying);
    }

    if (isPlaying && m_isLooping) return;

    system->playSound(m_sound, 0, false, &m_channel);
}

void Sound::Stop()
{
    if (m_channel)
    {
        m_channel->stop();
        m_channel = nullptr;
    }
}

void Sound::SetPaused(bool paused)
{
    if (m_channel)
    {
        m_channel->setPaused(paused);
    }
}

void Sound::SetVolume(float volume)
{
    if (m_channel)
    {
        m_channel->setVolume(volume);
    }
}

void Sound::SetPitch(float pitch)
{
    if (m_channel)
    {
        m_channel->setPitch(pitch);
    }
}

bool Sound::IsPlaying() const
{
    if (m_channel)
    {
        bool isPlaying = false;
        m_channel->isPlaying(&isPlaying);
        return isPlaying;
    }
    return false;
}