#pragma once
#include <Arduino.h>

struct SerwoBounds {
    int min;
    int max;
    int mid;
};

class LedcSerwoManager {
private:
    int chanel;
    int pin;
    int freq;
    int res;
    SerwoBounds bounds;
    const int default_position;
    volatile int current_postion;
    bool enabled;
    bool attached;

    static constexpr int MIN_CHANNEL = 0;
    static constexpr int MAX_CHANNEL = 15;
    static constexpr int MIN_ANGLE = 0;
    static constexpr int MAX_ANGLE = 180;
    static constexpr int DEFAULT_FREQ = 50;
    static constexpr int DEFAULT_RES = 16;
    static constexpr int DEFAULT_MIN_US = 500;
    static constexpr int DEFAULT_MID_US = 1500;
    static constexpr int DEFAULT_MAX_US = 2500;
    static constexpr unsigned long DEFAULT_MOVE_STEP_MS = 20;

    bool isValidPin(int _pin) const;
    bool isValidChannel(int _chanel) const;
    int clampAngle(int angle) const;
    int clampPulse(int pulseUs) const;
    int angleToPulseUs(int angle) const;
    uint32_t pulseUsToDuty(int pulseUs) const;
    void applyHardware();
    void writeDutyRaw(uint32_t duty);

public:
    LedcSerwoManager();
    ~LedcSerwoManager();

    int getchannel();
    int getUsedPin();
    int getFrequency();
    int getRes();
    SerwoBounds getSerwoBounds();
    const int getCurrentPosition();

    void begin(int _pin);
    void setPin(int _pin);
    void setChanel(int _chanel);
    void setFrequency(int _freq);
    void setRes(int _res);
    void detachLedc();
    void atachLedc();

    void setBounds(const SerwoBounds& newBounds);
    void setBounds(int minUs, int midUs, int maxUs);
    void setMinPosition(int minUs);
    void setMidPosition(int midUs);
    void setMaxPosition(int maxUs);

    bool isAttached() const;
    bool isEnabled() const;

    bool writePosition(int angle);
    bool writePulseMicros(int pulseUs);
    bool writeNormalized(float value);

    bool moveTo(int targetAngle, unsigned long durationMs = 0);
    bool moveToMin(unsigned long durationMs = 0);
    bool moveToMid(unsigned long durationMs = 0);
    bool moveToMax(unsigned long durationMs = 0);

    bool openLock(unsigned long durationMs = 0);
    bool closeLock(unsigned long durationMs = 0);

    void setCurrentPosition(int angle);
};