#pragma once
#include <algorithm>
#include <functional>

//  Pulse: 값만 관리 (최대/현재)
class Pulse {
public:
    // [수정] 시작 값을 받을 수 있도록 생성자 변경
    explicit Pulse(float max = 100.0f, float start = 100.0f)
        : pulse_max(max), pulse_value(start) {
    }

    float Value() const { return pulse_value; }
    float Max()   const { return pulse_max; }

    void setValue(float v) {
        if (v < 0.f)
            pulse_value = 0.f;
        else if (v > pulse_max)
            pulse_value = pulse_max;
        else
            pulse_value = v;
    }

    void setMax(float m) {
        pulse_max = (m < 0.f ? 0.f : m);
        if (pulse_value > pulse_max) pulse_value = pulse_max;
        if (pulse_value < 0.f)       pulse_value = 0.f;
    }

    void add(float amount) {
        if (amount <= 0.f) return;
        pulse_value = std::min(pulse_max, pulse_value + amount);
    }

    void spend(float amount) {
        if (amount <= 0.f) return;
        if (pulse_value >= amount)
            pulse_value -= amount;
    }

private:
    float pulse_max;
    float pulse_value;
};

struct PulseConfig {
    float chargeAmount = 30.f;
    float attackCost = 20.f;
};

struct PulseTickResult {
    float before = 0.f;
    float after = 0.f;
    float delta = 0.f;
    bool  charged = false;
    bool  spent = false;
    bool  spendFailed = false;
};

//기본적인 펄스 시스템 토대 
class PulseCore {
public:
    // [수정] 시작 값을 받을 수 있도록 생성자 변경
    explicit PulseCore(float maxPulse = 100.f, float startPulse = 100.f, PulseConfig cfg = {})
        : pulse(maxPulse, startPulse), config(cfg) {
    }

    Pulse& getPulse() { return pulse; }
    const Pulse& getPulse() const { return pulse; }
    PulseConfig& getConfig() { return config; }
    const PulseConfig& getConfig() const { return config; }

    PulseTickResult tick(bool pressE, bool pressQ,
        bool inCharger,
        bool inAttack)
    {
        PulseTickResult r{};
        r.before = pulse.Value();

        if (pressE && inCharger) {
            float before = pulse.Value();
            pulse.add(config.chargeAmount);
            r.delta += pulse.Value() - before;
            r.charged = true;
        }

        if (pressQ && inAttack) {
            float before = pulse.Value();
            float cost = config.attackCost;
            if (pulse.Value() >= cost) {
                pulse.spend(cost);
                r.delta -= cost;
                r.spent = true;
            }
            else {
                r.spendFailed = true;
            }
        }

        r.after = pulse.Value();
        return r;
    }

private:
    Pulse pulse;
    PulseConfig config;
};