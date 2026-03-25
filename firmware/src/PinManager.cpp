#include "PinManager.hpp"
#include <Arduino.h>

void PinKeypadController::update() {
    char key = keypad.getKey();

    if (state == PinState::BLOCKED) {
        processState();
        return;
    }

    if (key) {
        lastInputTime = millis();
        handleKey(key);
    }

    processState();
}

PinState PinKeypadController::getState() const {
    return state;
}

int PinKeypadController::getBlockedTime() const
{
    return blockedUntil;
}

void PinKeypadController::handleKey(char key) {
    if (state == PinState::BLOCKED) return;

    if (state == PinState::IDLE) {
        if (key == '*') {
            buffer = "";
            state = PinState::ENTERING;
            stateStartTime = millis();
        }
        return;
    }

    if (state == PinState::ENTERING) {
        if (key == '#') {
            if (buffer == correctPin) {
                state = PinState::VALID;
                stateStartTime = millis();
                incorrectTries = 0;
            } else {
                incorrectTries++;
                if (incorrectTries % 3 == 0) {
                    state = PinState::BLOCKED;
                    blockedUntil = millis() + calculateBlockTime();
                } else {
                    state = PinState::ERROR;
                    stateStartTime = millis();
                }
            }
            resetBuffer();
            return;
        }
        buffer += key;
        Serial.println(buffer);
    }

    if (state == PinState::SETTING_MODE) {
        if (key == '#') {
            state = PinState::CONFIRM_NEW_PIN;
            return;
        }
        buffer += key;
    }

    if (state == PinState::CONFIRM_NEW_PIN) {
        if (key == '#') {
            correctPin = buffer;
            resetBuffer();
            state = PinState::VALID;
        } else {
            buffer += key;
        }
    }
}

void PinKeypadController::processState() {
    unsigned long now = millis();

    if (state == PinState::BLOCKED) {
        if (now >= blockedUntil) {
            state = PinState::IDLE;
        }
        return;
    }

    if (state == PinState::ENTERING) {
        if (now - lastInputTime > inputTimeout) {
            resetBuffer();
            state = PinState::TIMEOUT;
            stateStartTime = millis();
        }
    }

    if (state == PinState::ERROR) {
        if (now - stateStartTime > 500) {
            state = PinState::IDLE;
        }
    }

    if (state == PinState::VALID) {
        if (now - stateStartTime < settingWindow) {
            if (keypad.getState() == HOLD && keypad.getKey() == '*') {
                state = PinState::SETTING_MODE;
                resetBuffer();
            }
        } else {
            state = PinState::IDLE;
        }
    }
    if(state == PinState::TIMEOUT){
        incorrectTries++;
         resetBuffer();
        state = PinState::IDLE;


    }
}

void PinKeypadController::resetBuffer() {
    buffer = "";
}

unsigned long PinKeypadController::calculateBlockTime() {
    int k = (incorrectTries / 3) - 1;
    if (k < 0) k = 0;
    return incorrectPinDelay * (1 << k);
}