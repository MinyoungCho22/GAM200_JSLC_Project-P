//Sound.cpp

#include "Sound.hpp"
#include "Logger.hpp"
#include <iostream>

// ==========================================================
// [Web / Emscripten] — SoLoud backend
// ==========================================================
#if defined(__EMSCRIPTEN__)

#include "soloud.h"
#include "soloud_wav.h"

SoundSystem& SoundSystem::Instance()
{
    static SoundSystem instance;
    return instance;
}

bool SoundSystem::Initialize()
{
    if (m_soloud) return true;

    m_soloud = new SoLoud::Soloud();
    SoLoud::result r = m_soloud->init();
    if (r != SoLoud::SO_NO_ERROR)
    {
        Logger::Instance().Log(Logger::Severity::Error, "SoLoud init failed: %d", r);
        delete m_soloud;
        m_soloud = nullptr;
        return false;
    }

    m_soloud->setGlobalVolume(m_masterVolume);
    Logger::Instance().Log(Logger::Severity::Info, "SoLoud Audio System Initialized (Web).");
    return true;
}

void SoundSystem::Update()
{
    // SoLoud runs on its own thread; nothing to do here.
}

void SoundSystem::Shutdown()
{
    if (m_soloud)
    {
        m_soloud->deinit();
        delete m_soloud;
        m_soloud = nullptr;
    }
}

void SoundSystem::SetMasterVolume(float volume)
{
    m_masterVolume = volume;
    if (m_soloud)
        m_soloud->setGlobalVolume(volume);
}

// ---- Sound ----

Sound::Sound() : m_wav(nullptr), m_handle(0), m_isLoaded(false), m_isLooping(false) {}

Sound::~Sound()
{
    Stop();
    delete m_wav;
    m_wav = nullptr;
}

Sound::Sound(Sound&& other) noexcept
    : m_wav(other.m_wav)
    , m_handle(other.m_handle)
    , m_volume(other.m_volume)
    , m_pitch(other.m_pitch)
    , m_isLoaded(other.m_isLoaded)
    , m_isLooping(other.m_isLooping)
{
    other.m_wav     = nullptr;
    other.m_handle  = 0;
    other.m_isLoaded  = false;
    other.m_isLooping = false;
}

Sound& Sound::operator=(Sound&& other) noexcept
{
    if (this == &other) return *this;

    Stop();
    delete m_wav;

    m_wav      = other.m_wav;
    m_handle   = other.m_handle;
    m_volume   = other.m_volume;
    m_pitch    = other.m_pitch;
    m_isLoaded  = other.m_isLoaded;
    m_isLooping = other.m_isLooping;

    other.m_wav     = nullptr;
    other.m_handle  = 0;
    other.m_isLoaded  = false;
    other.m_isLooping = false;
    return *this;
}

bool Sound::Load(const std::string& filepath, bool loop)
{
    if (!SoundSystem::Instance().GetSoLoud())
    {
        if (!SoundSystem::Instance().Initialize())
        {
            Logger::Instance().Log(Logger::Severity::Error, "SoLoud init failed; cannot load: %s", filepath.c_str());
            return false;
        }
    }

    delete m_wav;
    m_wav = new SoLoud::Wav();
    SoLoud::result r = m_wav->load(filepath.c_str());
    if (r != SoLoud::SO_NO_ERROR)
    {
        Logger::Instance().Log(Logger::Severity::Error, "SoLoud Wav::load failed (%s): %d", filepath.c_str(), r);
        delete m_wav;
        m_wav = nullptr;
        return false;
    }

    m_wav->setLooping(loop);
    m_isLooping = loop;
    m_isLoaded  = true;
    return true;
}

void Sound::Play()
{
    if (!m_isLoaded || !m_wav) return;

    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (!sl) return;

    // 루프 사운드가 이미 재생 중이면 다시 시작하지 않는다.
    if (m_isLooping && m_handle && sl->isValidVoiceHandle(m_handle)) return;

    m_handle = sl->play(*m_wav, m_volume);
    sl->setRelativePlaySpeed(m_handle, m_pitch);
}

void Sound::Stop()
{
    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (sl && m_handle)
    {
        sl->stop(m_handle);
        m_handle = 0;
    }
}

void Sound::SetPaused(bool paused)
{
    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (sl && m_handle)
        sl->setPause(m_handle, paused);
}

void Sound::SetVolume(float volume)
{
    m_volume = volume;
    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (sl && m_handle)
        sl->setVolume(m_handle, volume);
}

void Sound::SetPitch(float pitch)
{
    m_pitch = pitch;
    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (sl && m_handle)
        sl->setRelativePlaySpeed(m_handle, pitch);
}

bool Sound::IsPlaying() const
{
    SoLoud::Soloud* sl = SoundSystem::Instance().GetSoLoud();
    if (!sl || !m_handle) return false;
    return sl->isValidVoiceHandle(m_handle);
}

// ==========================================================
// [Native] — FMOD backend
// ==========================================================
#elif !defined(DISABLE_FMOD)
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

void SoundSystem::SetMasterVolume(float volume)
{
    m_masterVolume = volume;
    if (!m_system) return;

    FMOD::ChannelGroup* masterGroup = nullptr;
    if (m_system->getMasterChannelGroup(&masterGroup) == FMOD_OK && masterGroup)
    {
        masterGroup->setVolume(volume);
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

void SoundSystem::SetMasterVolume(float volume)
{
    m_masterVolume = volume;
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