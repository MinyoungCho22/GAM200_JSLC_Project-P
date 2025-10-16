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
    float chargeRatePerSecond = 50.f; // [수정] 초당 충전률
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

    // [수정] tick 함수에 double dt 인자 추가
    PulseTickResult tick(bool wantsToCharge, bool canCharge, bool isDashing, double dt)
    {
        PulseTickResult r{};
        r.before = pulse.Value();

        // 충전 로직
        if (wantsToCharge && canCharge) {
            float before = pulse.Value();
            // [수정] 시간에 비례하여 충전할 양을 계산
            float amount_to_add = config.chargeRatePerSecond * static_cast<float>(dt);
            pulse.add(amount_to_add);
            r.delta += pulse.Value() - before;
            r.charged = true;
        }

        // 대시 소모 로직 (현재는 매 프레임 소모. 추후 개선 가능)
        if (isDashing) {
            if (pulse.Value() >= config.dashCost) {
                // 이 로직은 1회성 소모가 더 적합할 수 있습니다.
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