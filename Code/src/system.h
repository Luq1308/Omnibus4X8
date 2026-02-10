#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SC8812A.h>
#include "config.h"

#define PSTOP_PIN 8
#define SDA_PIN 6
#define SCL_PIN 7
#define UP_PIN 1
#define ENTER_PIN 2
#define DOWN_PIN 3
#define EN_5V 5
#define EN_USB 4
#define EN_AC 10
#define FAN_PIN 20
#define INA219_ADDR 0x40
#define DS18B20_PIN 0

extern float vbat, ibat, soc;
extern float vbat_read, ibat_read, pbat_read;
extern float vbus_read, ibus_read, pbus_read;
extern float vcel_read;
extern float tempReadings[4];
extern float fanSpeed;
extern bool mpptActive;
extern bool apoCountingDown;

void systemSetup();
void readButtons();
bool getButtonState(int btn); 
void applySC8812AParams();
void readSensors();
void handleFanControl();
void handleAutoPowerOff();
void handleMPPT();
void handleNetwork();
void executeShutdown();
void applyPowerSettings();
void setupWiFi(int mode);
void handlePageScroll(bool up, bool down, bool enter);

#endif