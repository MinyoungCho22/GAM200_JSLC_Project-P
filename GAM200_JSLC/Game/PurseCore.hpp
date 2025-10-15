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
    float chargeRatePerSecond = 50.f; // [����] �ʴ� ������
    float attackCost = 20.f;
    float dashCost = 15.f;
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

    // [����] tick �Լ��� double dt ���� �߰�
    PulseTickResult tick(bool wantsToCharge, bool canCharge, bool isDashing, double dt)
    {
        PulseTickResult r{};
        r.before = pulse.Value();

        // ���� ����
        if (wantsToCharge && canCharge) {
            float before = pulse.Value();
            // [����] �ð��� ����Ͽ� ������ ���� ���
            float amount_to_add = config.chargeRatePerSecond * static_cast<float>(dt);
            pulse.add(amount_to_add);
            r.delta += pulse.Value() - before;
            r.charged = true;
        }

        // ��� �Ҹ� ���� (����� �� ������ �Ҹ�. ���� ���� ����)
        if (isDashing) {
            if (pulse.Value() >= config.dashCost) {
                // �� ������ 1ȸ�� �Ҹ� �� ������ �� �ֽ��ϴ�.
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