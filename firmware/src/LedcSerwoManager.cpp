#include "LedcSerwoManager.hpp"

LedcSerwoManager::LedcSerwoManager()
    : chanel(1),
      pin(-1),
      freq(DEFAULT_FREQ),
      res(DEFAULT_RES),
      bounds{DEFAULT_MIN_US, DEFAULT_MAX_US, DEFAULT_MID_US},
      default_position(0),
      current_postion(0),
      enabled(false),
      attached(false) {
}


LedcSerwoManager::~LedcSerwoManager() {
    detachLedc();
}

bool LedcSerwoManager::isValidPin(int _pin) const {
    return _pin >= 0 && _pin <= 39;
}

bool LedcSerwoManager::isValidChannel(int _chanel) const {
    return _chanel >= MIN_CHANNEL && _chanel <= MAX_CHANNEL;
}

int LedcSerwoManager::clampAngle(int angle) const {
    if (angle < MIN_ANGLE) return MIN_ANGLE;
    if (angle > MAX_ANGLE) return MAX_ANGLE;
    return angle;
}

int LedcSerwoManager::clampPulse(int pulseUs) const {
    if (pulseUs < 200) return 200;
    if (pulseUs > 3000) return 3000;
    return pulseUs;
}

int LedcSerwoManager::angleToPulseUs(int angle) const {
    angle = clampAngle(angle);
    long minUs = bounds.min;
    long maxUs = bounds.max;
    long pulse = minUs + ((maxUs - minUs) * angle) / 180L;
    return clampPulse((int)pulse);
}

uint32_t LedcSerwoManager::pulseUsToDuty(int pulseUs) const {
    pulseUs = clampPulse(pulseUs);
    if (freq <= 0 || res <= 0) return 0;

    const uint32_t maxDuty = (1UL << res) - 1UL;
    const uint32_t periodUs = 1000000UL / (uint32_t)freq;
    if (periodUs == 0) return 0;

    uint64_t duty = (uint64_t)pulseUs * (uint64_t)maxDuty / (uint64_t)periodUs;
    if (duty > maxDuty) duty = maxDuty;
    return (uint32_t)duty;
}

void LedcSerwoManager::applyHardware() {
    if (!isValidPin(pin) || !isValidChannel(chanel)) return;
    if (freq <= 0) freq = DEFAULT_FREQ;
    if (res <= 0) res = DEFAULT_RES;

    ledcSetup(chanel, freq, res);
    ledcAttachPin(pin, chanel);
    attached = true;
    enabled = true;
}

void LedcSerwoManager::writeDutyRaw(uint32_t duty) {
    if (!attached || !enabled) return;
    ledcWrite(chanel, duty);
}

int LedcSerwoManager::getchannel() {
    return chanel;
}

int LedcSerwoManager::getUsedPin() {
    return pin;
}

int LedcSerwoManager::getFrequency() {
    return freq;
}

int LedcSerwoManager::getRes() {
    return res;
}

SerwoBounds LedcSerwoManager::getSerwoBounds() {
    return bounds;
}

const int LedcSerwoManager::getCurrentPosition() {
    return current_postion;
}

void LedcSerwoManager::begin(int _pin) {
    if (_pin >= 0) {
        setPin(_pin);
    }

    if (!isValidPin(pin)) {
        return;
    }

    applyHardware();
    writePosition(default_position);
}

void LedcSerwoManager::setPin(int _pin) {
    if (!isValidPin(_pin)) {
        return;
    }

    if (attached) {
        ledcDetachPin(pin);
        attached = false;
    }

    pin = _pin;

    if (enabled) {
        applyHardware();
    }
}

void LedcSerwoManager::setChanel(int _chanel) {
    if (!isValidChannel(_chanel)) {
        return;
    }

    bool wasAttached = attached;
    if (attached) {
        ledcDetachPin(pin);
        attached = false;
    }

    chanel = _chanel;

    if (wasAttached || enabled) {
        applyHardware();
    }
}

void LedcSerwoManager::setFrequency(int _freq) {
    if (_freq <= 0) {
        return;
    }

    freq = _freq;

    if (attached) {
        applyHardware();
    }
}

void LedcSerwoManager::setRes(int _res) {
    if (_res <= 0 || _res > 20) {
        return;
    }

    res = _res;

    if (attached) {
        applyHardware();
    }
}

void LedcSerwoManager::detachLedc() {
    if (attached && isValidPin(pin)) {
        ledcDetachPin(pin);
    }
    attached = false;
    enabled = false;
}

void LedcSerwoManager::atachLedc() {
    if (!isValidPin(pin) || !isValidChannel(chanel)) {
        return;
    }

    applyHardware();
}

void LedcSerwoManager::setBounds(const SerwoBounds& newBounds) {
    bounds.min = clampPulse(newBounds.min);
    bounds.mid = clampPulse(newBounds.mid);
    bounds.max = clampPulse(newBounds.max);

    if (bounds.min > bounds.mid) bounds.mid = bounds.min;
    if (bounds.mid > bounds.max) bounds.mid = bounds.max;
    if (bounds.min > bounds.max) {
        int tmp = bounds.min;
        bounds.min = bounds.max;
        bounds.max = tmp;
    }
}

void LedcSerwoManager::setBounds(int minUs, int midUs, int maxUs) {
    SerwoBounds b{minUs, maxUs, midUs};
    setBounds(b);
}

void LedcSerwoManager::setMinPosition(int minUs) {
    bounds.min = clampPulse(minUs);
    if (bounds.mid < bounds.min) bounds.mid = bounds.min;
    if (bounds.max < bounds.mid) bounds.max = bounds.mid;
}

void LedcSerwoManager::setMidPosition(int midUs) {
    bounds.mid = clampPulse(midUs);
    if (bounds.mid < bounds.min) bounds.mid = bounds.min;
    if (bounds.mid > bounds.max) bounds.mid = bounds.max;
}

void LedcSerwoManager::setMaxPosition(int maxUs) {
    bounds.max = clampPulse(maxUs);
    if (bounds.max < bounds.mid) bounds.mid = bounds.max;
    if (bounds.min > bounds.mid) bounds.min = bounds.mid;
}

bool LedcSerwoManager::isAttached() const {
    return attached;
}

bool LedcSerwoManager::isEnabled() const {
    return enabled;
}

bool LedcSerwoManager::writePulseMicros(int pulseUs) {
    if (!attached || !enabled) return false;

    pulseUs = clampPulse(pulseUs);
    uint32_t duty = pulseUsToDuty(pulseUs);
    writeDutyRaw(duty);
    return true;
}

bool LedcSerwoManager::writePosition(int angle) {
    if (!attached || !enabled) return false;

    angle = clampAngle(angle);
    current_postion = angle;
    int pulseUs = angleToPulseUs(angle);
    return writePulseMicros(pulseUs);
}

bool LedcSerwoManager::writeNormalized(float value) {
    if (!attached || !enabled) return false;

    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    int angle = (int)(value * 180.0f + 0.5f);
    return writePosition(angle);
}

bool LedcSerwoManager::moveTo(int targetAngle, unsigned long durationMs) {
    if (!attached || !enabled) return false;

    targetAngle = clampAngle(targetAngle);
    int startAngle = clampAngle(current_postion);

    if (durationMs == 0 || startAngle == targetAngle) {
        return writePosition(targetAngle);
    }

    const unsigned long stepDelay = DEFAULT_MOVE_STEP_MS;
    unsigned long steps = durationMs / stepDelay;
    if (steps < 1) steps = 1;

    for (unsigned long i = 1; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        int angle = (int)((1.0f - t) * startAngle + t * targetAngle + 0.5f);
        writePosition(angle);
        delay(stepDelay);
    }

    return writePosition(targetAngle);
}

bool LedcSerwoManager::moveToMin(unsigned long durationMs) {
    return moveTo(0, durationMs);
}

bool LedcSerwoManager::moveToMid(unsigned long durationMs) {
    return moveTo(90, durationMs);
}

bool LedcSerwoManager::moveToMax(unsigned long durationMs) {
    return moveTo(180, durationMs);
}

bool LedcSerwoManager::openLock(unsigned long durationMs) {
    return moveToMax(durationMs);
}

bool LedcSerwoManager::closeLock(unsigned long durationMs) {
    return moveToMin(durationMs);
}

void LedcSerwoManager::setCurrentPosition(int angle) {
    current_postion = clampAngle(angle);
}