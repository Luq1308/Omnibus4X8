#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "system.h"

void setup() {
    Serial.begin(115200);
    configSetup();
    systemSetup();
    displaySetup();
    logStatus("System Booted");
}

void loop() {
    static unsigned long t50 = 0;
    static unsigned long t100 = 0;
    unsigned long now = millis();
    
    if (now - t50 >= 20) {
        t50 = now;
        readButtons();
        handleMenuLogic();
    }
    
    if (now - t100 >= 100) {
        t100 = now;
        readSensors();
        handleMPPT();
        handleNetwork();
        handleAutoPowerOff();
        handleFanControl();
        
        if (screenSelect == 0) drawStatusScreen();
        else if (screenSelect == 1) drawMenu();
        else if (screenSelect == 2) drawPage();
    }
}