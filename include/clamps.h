#include <Arduino.h>
#if USE_ESP32
#include <ESP32Servo.h>
#else
#include <Servo.h>
#endif
#include <pins.h>

#ifndef CLAMPS_H
#define CLAMPS_H
#endif

struct Vec2D {
    int x;
    int y;

    Vec2D(int x, int y) {
        this->x = x;
        this->y = y;
    }

    Vec2D() {
        this->x = 0;
        this->y = 0;
    }
};

class Clamps {
public:
    Clamps() {
        this->clamp1Servo.attach(CLAMP_1_SERVO_PIN);
        this->clamp2Servo.attach(CLAMP_2_SERVO_PIN);
    }

    Clamps(int clamp1Open, int clamp2Open, int clamp1Close, int clamp2Close) {
        this->clamp1Servo.attach(CLAMP_1_SERVO_PIN);
        this->clamp2Servo.attach(CLAMP_2_SERVO_PIN);
        this->clamp1Open = clamp1Open;
        this->clamp2Open = clamp2Open;
        this->clamp1Close = clamp1Close;
        this->clamp2Close = clamp2Close;
    }

    void openClamps() {
        this->clamp1Servo.write(this->clamp1Open);
        this->clamp2Servo.write(this->clamp2Open);
        clamp1Pos = clamp1Open;
        clamp2Pos = clamp2Open;
    }

    void closeClamps() {
        this->clamp1Servo.write(this->clamp1Close);
        this->clamp2Servo.write(this->clamp2Close);
        clamp1Pos = clamp1Close;
        clamp2Pos = clamp2Close;
    }

    void write(int clamp1, int clamp2) {
        this->clamp1Servo.write(clamp1);
        this->clamp2Servo.write(clamp2);
        clamp1Pos = clamp1;
        clamp2Pos = clamp2;
    }

    Vec2D getPos() {
        return Vec2D(clamp1Pos, clamp2Pos);
    }

    void nudge(int clamp1, int clamp2) {
        clamp1 = constrain(clamp1, -1, 1);
        clamp2 = constrain(clamp2, -1, 1);
        this->clamp1Servo.write(clamp1Pos + clamp1);
        this->clamp2Servo.write(clamp2Pos + clamp2);
        clamp1Pos += clamp1;
        clamp2Pos += clamp2;
    }


private:
    Servo clamp1Servo;
    Servo clamp2Servo;

    int clamp1Pos = 0;
    int clamp2Pos = 0;

    int clamp1Open = 0;
    int clamp2Open = 0;

    int clamp1Close = 180;
    int clamp2Close = 180;
};
