#pragma once
#include <algorithm>
#include <functional>

class Pulse {
public:
    explicit Pulse(float max = 100.0f, float start = 100.0f)
        : pulse_max(max), pulse_value(start) {
    }

    float Value() const { return pulse_value; }
    float Max()   const { return pulse_max; }

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
    float dashCost = 15.f; // [�߰�] ��� �Ҹ�
};

struct PulseTickResult {
    float before = 0.f;
    float after = 0.f;
    float delta = 0.f;
    bool  charged = false;
    bool  spent = false;
    bool  spendFailed = false;
};

class PulseCore {
public:
    explicit PulseCore(float maxPulse = 100.f, float startPulse = 100.f, PulseConfig cfg = {})
        : pulse(maxPulse, startPulse), config(cfg) {
    }

    Pulse& getPulse() { return pulse; }
    const Pulse& getPulse() const { return pulse; }
    PulseConfig& getConfig() { return config; }
    const PulseConfig& getConfig() const { return config; }

    // [����] tick �Լ� ���� ���� (isDashing �߰�, attack ���� ���� ����)
    PulseTickResult tick(bool wantsToCharge, bool canCharge, bool isDashing)
    {
        PulseTickResult r{};
        r.before = pulse.Value();

        // ���� ����
        if (wantsToCharge && canCharge) {
            float before = pulse.Value();
            pulse.add(config.chargeAmount);
            r.delta += pulse.Value() - before;
            r.charged = true;
        }

        // ��� �Ҹ� ����
        if (isDashing) {
            if (pulse.Value() >= config.dashCost) {
                pulse.spend(config.dashCost);
                r.delta -= config.dashCost;
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