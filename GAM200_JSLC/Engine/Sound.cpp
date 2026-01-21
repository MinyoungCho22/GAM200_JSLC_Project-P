//Sound.cpp

#include "Sound.hpp"
#include "Logger.hpp"
#include <iostream>

#ifndef DISABLE_FMOD
#include "fmod_errors.h"

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
        Logger::Instance().Log(Logger::Severity::Error, "FMOD System_Create failed: %s", FMOD_ErrorString(result));
        m_system = nullptr;
        return false;
    }

    result = m_system->init(512, FMOD_INIT_NORMAL, m_extraDriverData);
    if (result != FMOD_OK)
    {
        Logger::Instance().Log(Logger::Severity::Error, "FMOD system->init failed: %s", FMOD_ErrorString(result));
        m_system->release();
        m_system = nullptr;
        return false;
    }

    Logger::Instance().Log(Logger::Severity::Info, "FMOD Audio System Initialized.");
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

Sound::Sound(Sound&& other) noexcept
    : m_sound(other.m_sound)
    , m_channel(other.m_channel)
    , m_isLoaded(other.m_isLoaded)
    , m_isLooping(other.m_isLooping)
{
    other.m_sound = nullptr;
    other.m_channel = nullptr;
    other.m_isLoaded = false;
    other.m_isLooping = false;
}

Sound& Sound::operator=(Sound&& other) noexcept
{
    if (this == &other) return *this;

    // Stop current channel if playing
    if (m_channel)
    {
        m_channel->stop();
        m_channel = nullptr;
    }

    // release current sound
    if (m_sound != nullptr)
    {
        m_sound->release();
        m_sound = nullptr;
    }
    
    m_isLoaded = false;
    m_isLooping = false;

    // steal
    m_sound = other.m_sound;
    m_channel = other.m_channel;
    m_isLoaded = other.m_isLoaded;
    m_isLooping = other.m_isLooping;

    other.m_sound = nullptr;
    other.m_channel = nullptr;
    other.m_isLoaded = false;
    other.m_isLooping = false;
    return *this;
}

Sound::~Sound()
{
    if (m_sound != nullptr)
    {
        m_sound->release();
        m_sound = nullptr;
    }
}

bool Sound::Load(const std::string& filepath, bool loop)
{
    if (!SoundSystem::Instance().GetSystem())
    {
        if (!SoundSystem::Instance().Initialize())
        {
            Logger::Instance().Log(Logger::Severity::Error, "FMOD init failed; cannot load sound: %s", filepath.c_str());
            m_isLoaded = false;
            return false;
        }
    }

    FMOD::System* system = SoundSystem::Instance().GetSystem();
    if (!system)
    {
        Logger::Instance().Log(Logger::Severity::Error, "FMOD system is null; cannot load sound: %s", filepath.c_str());
        m_isLoaded = false;
        return false;
    }
    FMOD_RESULT result;

    FMOD_MODE mode = FMOD_DEFAULT;
    mode |= (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    m_isLooping = loop;

    result = system->createSound(filepath.c_str(), mode, 0, &m_sound);
    if (result != FMOD_OK)
    {
        Logger::Instance().Log(Logger::Severity::Error, "FMOD createSound failed (%s): %s", filepath.c_str(), FMOD_ErrorString(result));
        return false;
    }

    m_isLoaded = true;
    return true;
}

void Sound::Play()
{
    if (!m_isLoaded || !m_sound) return;

    FMOD::System* system = SoundSystem::Instance().GetSystem();
    if (!system)
    {
        Logger::Instance().Log(Logger::Severity::Error, "FMOD system is null; cannot play sound");
        return;
    }

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

#else

SoundSystem& SoundSystem::Instance()
{
    static SoundSystem instance;
    return instance;
}

bool SoundSystem::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "FMOD disabled; audio will not play.");
    return true;
}

void SoundSystem::Update()
{
    // Audio disabled; nothing to update.
}

void SoundSystem::Shutdown()
{
    // Audio disabled; nothing to shut down.
}

Sound::Sound() : m_sound(nullptr), m_channel(nullptr), m_isLoaded(false)
{
}

Sound::Sound(Sound&& other) noexcept
    : m_sound(other.m_sound)
    , m_channel(other.m_channel)
    , m_isLoaded(other.m_isLoaded)
    , m_isLooping(other.m_isLooping)
{
    other.m_sound = nullptr;
    other.m_channel = nullptr;
    other.m_isLoaded = false;
    other.m_isLooping = false;
}

Sound& Sound::operator=(Sound&& other) noexcept
{
    if (this == &other) return *this;
    m_sound = other.m_sound;
    m_channel = other.m_channel;
    m_isLoaded = other.m_isLoaded;
    m_isLooping = other.m_isLooping;
    other.m_sound = nullptr;
    other.m_channel = nullptr;
    other.m_isLoaded = false;
    other.m_isLooping = false;
    return *this;
}

Sound::~Sound() = default;

bool Sound::Load(const std::string& filepath, bool loop)
{
    (void)filepath;
    m_isLooping = loop;
    m_isLoaded = true;
    return true;
}

void Sound::Play() {}
void Sound::Stop() { m_channel = nullptr; }
void Sound::SetPaused(bool /*paused*/) {}
void Sound::SetVolume(float /*volume*/) {}
void Sound::SetPitch(float /*pitch*/) {}

bool Sound::IsPlaying() const
{
    return false;
}

#endif