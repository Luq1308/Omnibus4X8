#include "system.h"
#include "display.h"

INA219 INA(INA219_ADDR);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
SC8812A sc8812(PSTOP_PIN);

DeviceAddress tempSensors[4] = {
  { 0x28, 0x83, 0xC2, 0x01, 0x00, 0x02, 0x24, 0x50 },
  { 0x28, 0xAA, 0xA1, 0x02, 0x00, 0x02, 0x24, 0xA7 },
  { 0x28, 0x26, 0x30, 0x03, 0x00, 0x02, 0x24, 0xC5 },
  { 0x28, 0x2C, 0x97, 0x02, 0x00, 0x02, 0x24, 0xE7 }
};

float vbat = 0, ibat = 0, soc = 0;
float vbat_read = 0, ibat_read = 0, pbat_read = 0;
float vbus_read = 0, ibus_read = 0, pbus_read = 0;
float vcel_read = 0;
float tempReadings[4] = {0};
float fanSpeed = 0;
bool mpptActive = false;
bool apoCountingDown = false;

bool btnStates[4] = {0}; 
byte pinStates[3] = {1, 1, 1};
unsigned long enterPressTime = 0;
bool enterLongHandled = false;

int currentWifiState = -1;
int pageScrollY = 0;

void systemSetup() {
    pinMode(UP_PIN, INPUT_PULLUP);
    pinMode(DOWN_PIN, INPUT_PULLUP);
    pinMode(ENTER_PIN, INPUT_PULLUP);
    pinMode(EN_USB, OUTPUT);
    pinMode(EN_5V, OUTPUT);
    pinMode(EN_AC, OUTPUT);
    pinMode(PSTOP_PIN, OUTPUT);
    
    digitalWrite(PSTOP_PIN, HIGH);
    digitalWrite(EN_5V, HIGH);
    
    ledcSetup(0, 10000, 8);
    ledcAttachPin(FAN_PIN, 0);
    
    setupWiFi(wifi_mode_index);
}

void readButtons() {
    btnStates[0] = !digitalRead(UP_PIN);
    btnStates[1] = !digitalRead(DOWN_PIN);
    byte enterVal = digitalRead(ENTER_PIN);
    
    if (enterVal == LOW && pinStates[1] == HIGH) {
        enterPressTime = millis();
        enterLongHandled = false;
    }
    
    if (enterVal == LOW && !enterLongHandled && (millis() - enterPressTime > 600)) {
        btnStates[3] = true;
        enterLongHandled = true;
    } else {
        btnStates[3] = false;
    }

    if (enterVal == HIGH && pinStates[1] == LOW && !enterLongHandled) {
        btnStates[2] = true;
    } else {
        btnStates[2] = false;
    }
    
    pinStates[1] = enterVal;
}

bool getButtonState(int btn) {
    return btnStates[btn];
}

void applySC8812AParams() {
    sc8812.setShuntResistors(5.0f, 5.0f);
    sc8812.setCellCount(3);
    sc8812.setSwitchingFrequency(0);
    sc8812.enablePFMMode(true);
    sc8812.enableCurrentFoldback(false);
    sc8812.setCellVoltage((uint8_t)sc_charge_volt_index);
    sc8812.setIBATCurrentLimit(sc_ibat_limit);
}

void readSensors() {
    static bool initiate = true;
    if(initiate == true) {
        initiate = false;
        INA.begin();
        INA.setMaxCurrentShunt(30.0, 0.005);
        INA.setGain(4);
        INA.setBusSamples(7);
        INA.setShuntSamples(7);
        ds18b20.begin();
        ds18b20.requestTemperatures();
        applySC8812AParams();
        sc8812.enableADC(true);
    }
    vbat_read = INA.getBusVoltage();
    ibat_read = INA.getCurrent();
    if (ibat_read > -0.002 && ibat_read < 0.002) ibat_read = 0;
    pbat_read = vbat_read * ibat_read;
    vbus_read = sc8812.getVbusVoltage();
    ibus_read = sc8812.getIbusCurrent();
    pbus_read = vbus_read * ibus_read;
    vcel_read = vbat_read / 4.0;
    
    if (millis() % 2000 < 100) ds18b20.requestTemperatures();
    for (int i=0; i<4; i++) {
        float t = ds18b20.getTempC(tempSensors[i]);
        if (t > -50) tempReadings[i] = t;
    }
    
    float v_comp = vbat_read + (ibat_read * -(cal_sag_comp / 10));
    if (v_comp >= cal_max_soc_vcel * 4) soc = 100.0;
    else if (v_comp <= cal_min_soc_vcel * 4) soc = 0.0;
    else soc = ((v_comp - (cal_min_soc_vcel * 4)) / ((cal_max_soc_vcel * 4) - (cal_min_soc_vcel * 4))) * 100.0;
}

void executeShutdown() {
    sc8812.enableADC(false);
    digitalWrite(EN_5V, LOW);
    esp_deep_sleep_enable_gpio_wakeup(1ULL << ENTER_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}

void applyPowerSettings() {
    digitalWrite(EN_USB, qm_usb_out);
    digitalWrite(EN_AC, qm_ac_out);
    
    sc8812.disablePower();
    mpptActive = false;
    
    if (qm_dc_mode_index == 1) { 
        sc8812.setVBUSVoltage(qm_dc_vbus);
        sc8812.setIBUSCurrentLimit(qm_dc_ibus);
        sc8812.enableDischarge();
    } else if (qm_dc_mode_index == 2) { 
        sc8812.setMinVBUSVoltage(qm_dc_vbus);
        sc8812.setIBUSCurrentLimit(qm_dc_ibus);
        sc8812.enableCharge();
    } else if (qm_dc_mode_index == 3) {
        mpptActive = true;
        sc8812.setIBUSCurrentLimit(qm_dc_ibus);
        sc8812.enableCharge();
    }
}

void setupWiFi(int mode) {
    if (mode == currentWifiState) return;
    
    WiFi.disconnect(true);
    WiFi.persistent(false); 
    WiFi.mode(WIFI_OFF);
    delay(100);

    if (mode == 1) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifi_sta_ssid, wifi_sta_pass);
        ArduinoOTA.begin();
    } else if (mode == 2) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifi_ap_ssid, wifi_ap_pass);
        delay(100); 
        ArduinoOTA.begin();
    }
    currentWifiState = mode;
}

void handleNetwork() {
    if (currentWifiState > 0) ArduinoOTA.handle();
}

long calcPWM(float temp, float minT, float maxT, int minP) {
    if (temp < minT) return 0;

    if (temp >= maxT) return 100;

    return map((long)(temp * 100), (long)(minT * 100), (long)(maxT * 100), (long)minP, 100L);
}

void handleFanControl() {
    static unsigned long startT = 0;

    if (fan_test_startup) {
        if (startT == 0) startT = millis();
        if (millis() - startT < 2000) {
            ledcWrite(0, 255);
            fanSpeed = 1.0;
            return;
        }
    }

    long pwmBat = calcPWM(tempReadings[0], tbat_min, tbat_max, fan_min_pwm);
    long pwmMod = calcPWM(max(tempReadings[1], tempReadings[2]), tmod_min, tmod_max, fan_min_pwm);
    long pwmInv = calcPWM(tempReadings[3], tinv_min, tinv_max, fan_min_pwm);
    
    long pwm = max(pwmBat, max(pwmMod, pwmInv));
    ledcWrite(0, (int)(pwm * 2.55));

    if (pwm > 0) {
        if (pwm >= 100) {
            fanSpeed = 1.0;
        } else {
            float range = 100.0 - (float)fan_min_pwm;
            if (range > 0) {
                fanSpeed = ((float)pwm - (float)fan_min_pwm) / range;
                fanSpeed = constrain(fanSpeed, 0.01, 1.0); 
            } else {
                fanSpeed = 1.0;
            }
        }
    } else {
        fanSpeed = 0.0;
    }
}

void handleAutoPowerOff() {
    static unsigned long lastAct = 0;
    if (!apo_enable) {
        apoCountingDown = false;
        return;
    }
    
    bool active = false;
    float effective_thresh_a = (qm_ac_out ? apo_ac_thres : apo_curr_thres) / 1000.0;

    if (ibat_read > 0.1 || abs(ibat_read) > effective_thresh_a) active = true;
    if (btnStates[0] || btnStates[1] || btnStates[2]) active = true;
    if (tempReadings[0] > tbat_min) active = true;
    if (max(tempReadings[1], tempReadings[2]) > tmod_min) active = true;
    if (tempReadings[3] > tinv_min) active = true;
    
    if (active) {
        lastAct = millis();
        apoCountingDown = false;
    } else {
        apoCountingDown = true;
    }
    
    if (millis() - lastAct > (apo_delay * 60000)) executeShutdown();
}

void handleMPPT() {
    if (!mpptActive) return;
    static unsigned long lastRun = 0;
    if (millis() - lastRun < (mppt_interval * 1000)) return;
    lastRun = millis();
    
    static float targetV = mppt_start_volt;
    static float lastP = 0;
    float currP = vbus_read * ibus_read;
    
    if (currP < lastP) mppt_step = -mppt_step;
    targetV += mppt_step;
    targetV = constrain(targetV, mppt_min_volt, mppt_max_volt);
    sc8812.setMinVBUSVoltage(targetV);
    lastP = currP;
}

void handlePageScroll(bool up, bool down, bool enter) {
    if (enter) {
        screenSelect = 1; 
        pageScrollY = 0;
    }
    if (up && pageScrollY > 0) pageScrollY--;
    if (down) pageScrollY++;
}