#ifndef PINMANAGER_HPP
#define PINMANAGER_HPP

#include <Keypad.h>

enum class PinState {
    IDLE,
    ENTERING,
    VALID,
    ERROR,
    TIMEOUT,
    SETTING_MODE,
    CONFIRM_NEW_PIN,
    BLOCKED
};

class PinKeypadController {
public:
    PinKeypadController(Keypad& _keypad) : keypad(_keypad) {};

    void update();
    PinState getState() const;
    int getBlockedTime() const;

private:
    Keypad& keypad;
    String correctPin = "1234";
    String buffer = "";
    PinState state = PinState::IDLE;

    int incorrectTries = 0;
    bool lockBlocked = false;
    bool starHeld = false;

    unsigned long lastInputTime = 0;
    unsigned long stateStartTime = 0;
    unsigned long blockedUntil = 0;

    const unsigned long inputTimeout = 15000;
    const unsigned long settingWindow = 5000;
    const unsigned long incorrectPinDelay = 30000;

    void handleKey(char key);
    void processState();
    void resetBuffer();
    unsigned long calculateBlockTime();
};

#endif