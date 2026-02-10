#include "config.h"
#include "system.h"
#include <vector>

Preferences preferences;
std::vector<String> statusLogs;
const int MAX_LOGS = 30;

// --- Variables ---
bool qm_usb_out = false;
bool qm_ac_out = false;
int qm_dc_mode_index = 0;
float qm_dc_vbus = 12.0;
float qm_dc_ibus = 2.0;

bool apo_enable = true;
int apo_curr_thres = 20;
int apo_ac_thres = 150;
int apo_delay = 5;

int sc_charge_volt_index = 1;
float sc_ibat_limit = 8.0;

float mppt_start_volt = 14.0;
float mppt_min_volt = 12.0;
float mppt_max_volt = 18.0;
float mppt_step = 0.2;
float mppt_interval = 0.5;

bool fan_test_startup = true;
int fan_min_pwm = 50;
float tbat_min = 40.0;
float tbat_max = 50.0;
float tmod_min = 45.0;
float tmod_max = 65.0;
float tinv_min = 45.0;
float tinv_max = 60.0;

float cal_min_soc_vcel = 3.2;
float cal_max_soc_vcel = 4.0;
float cal_sag_comp = 0.30;

int wifi_mode_index = 0;
bool sys_beeper = true;

// --- WiFi Definitions ---
const char* wifi_sta_ssid = "YOUR_WIFI_SSID";
const char* wifi_sta_pass = "YOUR_WIFI_PASSWORD";
const char* wifi_ap_ssid = "Omnibus 4X8";
const char* wifi_ap_pass = "12345678";
const char* web_address = "omnibus.local";

// --- Menu State ---
int screenSelect = 0; 
int statusViewIndex = 0; 
int activePageId = 0; 
MenuItem* currentMenu;
int currentMenuSize;
int cursorPosition = 0;
int viewPosition = 0;
bool isEditing = false;
bool isQuickMenuEditing = false;
int quickMenuCursor = 0;

MenuState menuStack[5];
int stackLevel = -1;
unsigned long holdStartTime = 0;
unsigned long lastValueChangeTime = 0;

const char* dcModeOptions[] = {"OFF", "OUT", "IN", "MPPT"};
const char* chargeVoltOptions[] = {"4.10", "4.20", "4.25"};
const char* wifiOptions[] = {"OFF", "STA", "AP"};

// --- Actions ---
void actionExit();
void actionBack();
void actionShutdown();
void actionRestore();
void openPageCreds();
void openPageLogs();
void openPageAbout();

void changeValue(MenuItem* item, bool increase) {
    if (!item->variable) return;

    int multiplier = 1;

    if (holdStartTime != 0 && (item->type == ITEM_INT || item->type == ITEM_FLOAT)) {
        unsigned long holdDuration = millis() - holdStartTime;
        if (holdDuration > 1500) multiplier = 20;
        else if (holdDuration > 750) multiplier = 5;
    }

    float effectiveMin = item->min;
    float effectiveMax = item->max;

    if (item->dynMin != nullptr) {
        if (item->type == ITEM_FLOAT) effectiveMin = max(effectiveMin, *(float*)item->dynMin);
        else if (item->type == ITEM_INT) effectiveMin = max(effectiveMin, (float)*(int*)item->dynMin);
    }
    if (item->dynMax != nullptr) {
        if (item->type == ITEM_FLOAT) effectiveMax = min(effectiveMax, *(float*)item->dynMax);
        else if (item->type == ITEM_INT) effectiveMax = min(effectiveMax, (float)*(int*)item->dynMax);
    }

    if (item->type == ITEM_BOOL) {
        bool* val = (bool*)item->variable;
        *val = !(*val);
    } 
    else if (item->type == ITEM_INT) {
        int* val = (int*)item->variable;
        int step = max(1, (int)(item->step * multiplier));
        
        if (increase) *val = min((int)effectiveMax, *val + step);
        else *val = max((int)effectiveMin, *val - step);
    } 
    else if (item->type == ITEM_FLOAT) {
        float* val = (float*)item->variable;
        float step = item->step * (float)multiplier;
        
        if (increase) *val = min(effectiveMax, *val + step);
        else *val = max(effectiveMin, *val - step);
    } 
    else if (item->type == ITEM_STRING) {
        if (holdStartTime != 0 && (millis() - holdStartTime) > 100) return;
        int* val = (int*)item->variable;

        if (increase) *val = (*val < item->numOptions - 1) ? *val + 1 : 0;
        else *val = (*val > 0) ? *val - 1 : item->numOptions - 1;
    }
}

// --- Menu Definitions ---

MenuItem menu_restore[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Confirm Restore", ITEM_ACTION, nullptr, (void*)actionRestore}
};

MenuItem menu_sys[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Enable Beeper", ITEM_BOOL, &sys_beeper, nullptr, 0, 0, 0, nullptr, 0, true, "beep"},
    {"Status Logs", ITEM_ACTION, nullptr, (void*)openPageLogs},
    {"Restore Defaults", ITEM_MENU, nullptr, menu_restore, 0, 0, 0, nullptr, 2},
    {"About", ITEM_ACTION, nullptr, (void*)openPageAbout}
};

MenuItem menu_wifi[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Wi-Fi Mode", ITEM_STRING, &wifi_mode_index, nullptr, 0, 0, 0, wifiOptions, 3, true, "wifi"},
    {"Wi-Fi Credentials", ITEM_ACTION, nullptr, (void*)openPageCreds}
};

MenuItem menu_cal[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Min SOC Voltage (V)", ITEM_FLOAT, &cal_min_soc_vcel, nullptr, 2.5, 4.3, 0.01, nullptr, 0, true, "c_min", nullptr, &cal_max_soc_vcel},
    {"Max SOC Voltage (V)", ITEM_FLOAT, &cal_max_soc_vcel, nullptr, 2.5, 4.3, 0.01, nullptr, 0, true, "c_max", &cal_min_soc_vcel, nullptr},
    {"Sag Compensation", ITEM_FLOAT, &cal_sag_comp, nullptr, 0.01, 1.00, 0.01, nullptr, 0, true, "c_sag"}
};

MenuItem menu_temp[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Startup Fan Test", ITEM_BOOL, &fan_test_startup, nullptr, 0, 0, 0, nullptr, 0, true, "f_test"},
    {"Fan Min PWM %", ITEM_INT, &fan_min_pwm, nullptr, 0, 75, 1, nullptr, 0, true, "f_min"},
    {"TBAT Min Temp (C)", ITEM_FLOAT, &tbat_min, nullptr, 25.0, 60.0, 1.0, nullptr, 0, true, "tb_min", nullptr, &tbat_max},
    {"TBAT Max Temp (C)", ITEM_FLOAT, &tbat_max, nullptr, 25.0, 60.0, 1.0, nullptr, 0, true, "tb_max", &tbat_min, nullptr},
    {"TMOD Min Temp (C)", ITEM_FLOAT, &tmod_min, nullptr, 25.0, 90.0, 1.0, nullptr, 0, true, "tm_min", nullptr, &tmod_max},
    {"TMOD Max Temp (C)", ITEM_FLOAT, &tmod_max, nullptr, 25.0, 90.0, 1.0, nullptr, 0, true, "tm_max", &tmod_min, nullptr},
    {"TINV Min Temp (C)", ITEM_FLOAT, &tinv_min, nullptr, 25.0, 90.0, 1.0, nullptr, 0, true, "ti_min", nullptr, &tinv_max},
    {"TINV Max Temp (C)", ITEM_FLOAT, &tinv_max, nullptr, 25.0, 90.0, 1.0, nullptr, 0, true, "ti_max", &tinv_min, nullptr}
};

MenuItem menu_mppt[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Start Voltage (V)", ITEM_FLOAT, &mppt_start_volt, nullptr, 7.0, 18.0, 0.1, nullptr, 0, true, "m_start"},
    {"Min Voltage (V)", ITEM_FLOAT, &mppt_min_volt, nullptr, 5.0, 20.0, 0.1, nullptr, 0, true, "m_min", nullptr, &mppt_max_volt},
    {"Max Voltage (V)", ITEM_FLOAT, &mppt_max_volt, nullptr, 5.0, 20.0, 0.1, nullptr, 0, true, "m_max", &mppt_min_volt, nullptr},
    {"Perturb Step (V)", ITEM_FLOAT, &mppt_step, nullptr, 0.1, 1.0, 0.1, nullptr, 0, true, "m_step"},
    {"Perturb Interval (s)", ITEM_FLOAT, &mppt_interval, nullptr, 0.1, 2.0, 0.1, nullptr, 0, true, "m_int"}
};

MenuItem menu_sc[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Charge Voltage (V)", ITEM_STRING, &sc_charge_volt_index, nullptr, 0, 0, 0, chargeVoltOptions, 3, true, "sc_v"},
    {"IBAT Limit (A)", ITEM_FLOAT, &sc_ibat_limit, nullptr, 2.0, 12.0, 0.1, nullptr, 0, true, "sc_i"}
};

MenuItem menu_apo[] = {
    {"Back", ITEM_BACK, nullptr, (void*)actionBack},
    {"Enable APO", ITEM_BOOL, &apo_enable, nullptr, 0, 0, 0, nullptr, 0, true, "apo_en"},
    {"APO Thres (mA)", ITEM_INT, &apo_curr_thres, nullptr, 5, 200, 5, nullptr, 0, true, "apo_th"},
    {"APO AC Thres (mA)", ITEM_INT, &apo_ac_thres, nullptr, 5, 500, 5, nullptr, 0, true, "apo_ac"},
    {"APO Delay (mins)", ITEM_INT, &apo_delay, nullptr, 1, 60, 1, nullptr, 0, true, "apo_del"}
};

MenuItem mainMenu[] = {
    {"Exit", ITEM_ACTION, nullptr, (void*)actionExit},
    {"Auto Power Off", ITEM_MENU, nullptr, menu_apo, 0, 0, 0, nullptr, 5}, 
    {"SC8812A Parameters", ITEM_MENU, nullptr, menu_sc, 0, 0, 0, nullptr, 3},
    {"MPPT Parameters", ITEM_MENU, nullptr, menu_mppt, 0, 0, 0, nullptr, 6},
    {"Temperature Control", ITEM_MENU, nullptr, menu_temp, 0, 0, 0, nullptr, 9},
    {"Calibration", ITEM_MENU, nullptr, menu_cal, 0, 0, 0, nullptr, 4},
    {"Wi-Fi", ITEM_MENU, nullptr, menu_wifi, 0, 0, 0, nullptr, 3},
    {"System Settings", ITEM_MENU, nullptr, menu_sys, 0, 0, 0, nullptr, 5}
};

MenuItem quickMenu[] = {
    {"Menu", ITEM_ACTION, nullptr, (void*)actionExit},
    {"USB", ITEM_BOOL, &qm_usb_out, nullptr},
    {"AC", ITEM_BOOL, &qm_ac_out, nullptr},
    {"DC-M", ITEM_STRING, &qm_dc_mode_index, nullptr, 0, 0, 0, dcModeOptions, 4},
    {"DC-V", ITEM_FLOAT, &qm_dc_vbus, nullptr, 1.0, 20.0, 0.1, nullptr, 0, true, "dcv"},
    {"DC-I", ITEM_FLOAT, &qm_dc_ibus, nullptr, 0.3, 6.0, 0.1, nullptr, 0, true, "dci"},
    {"Shutdown", ITEM_ACTION, nullptr, (void*)actionShutdown}
};

void logStatus(const char* msg) {
    if (statusLogs.size() >= MAX_LOGS) {
        statusLogs.erase(statusLogs.begin());
    }
    statusLogs.push_back(String(msg));
}

String getLogLine(int index) {
    if (index >= 0 && index < statusLogs.size()) return statusLogs[index];
    return "";
}

int getLogCount() {
    return statusLogs.size();
}

void saveSetting(const char* key, void* val, ItemType type) {
    if (!key) return;
    if (type == ITEM_BOOL) preferences.putBool(key, *(bool*)val);
    else if (type == ITEM_INT) preferences.putInt(key, *(int*)val);
    else if (type == ITEM_FLOAT) preferences.putFloat(key, *(float*)val);
    else if (type == ITEM_STRING) preferences.putInt(key, *(int*)val);
}

void loadSettings() {
    preferences.begin("settings", false);
    qm_dc_vbus = preferences.getFloat("dcv", qm_dc_vbus);
    qm_dc_ibus = preferences.getFloat("dci", qm_dc_ibus);
    apo_enable = preferences.getBool("apo_en", apo_enable);
    apo_curr_thres = preferences.getInt("apo_th", apo_curr_thres);
    apo_ac_thres = preferences.getInt("apo_ac", apo_ac_thres);
    apo_delay = preferences.getInt("apo_del", apo_delay);
    sc_charge_volt_index = preferences.getInt("sc_v", sc_charge_volt_index);
    sc_ibat_limit = preferences.getFloat("sc_i", sc_ibat_limit);
    mppt_start_volt = preferences.getFloat("m_start", mppt_start_volt);
    mppt_min_volt = preferences.getFloat("m_min", mppt_min_volt);
    mppt_max_volt = preferences.getFloat("m_max", mppt_max_volt);
    mppt_step = preferences.getFloat("m_step", mppt_step);
    mppt_interval = preferences.getFloat("m_int", mppt_interval);
    fan_test_startup = preferences.getBool("f_test", fan_test_startup);
    fan_min_pwm = preferences.getInt("f_min", fan_min_pwm);
    tbat_min = preferences.getFloat("tb_min", tbat_min);
    tbat_max = preferences.getFloat("tb_max", tbat_max);
    tmod_min = preferences.getFloat("tm_min", tmod_min);
    tmod_max = preferences.getFloat("tm_max", tmod_max);
    tinv_min = preferences.getFloat("ti_min", tinv_min);
    tinv_max = preferences.getFloat("ti_max", tinv_max);
    cal_min_soc_vcel = preferences.getFloat("c_min", cal_min_soc_vcel);
    cal_max_soc_vcel = preferences.getFloat("c_max", cal_max_soc_vcel);
    cal_sag_comp = preferences.getFloat("c_sag", cal_sag_comp);
    wifi_mode_index = preferences.getInt("wifi", wifi_mode_index);
    sys_beeper = preferences.getBool("beep", sys_beeper);
}

void configSetup() {
    loadSettings();
    currentMenu = mainMenu;
    currentMenuSize = sizeof(mainMenu) / sizeof(MenuItem);
}

void actionExit() {
    if (screenSelect == 0) screenSelect = 1;
    else screenSelect = 0;
    isEditing = false;
    isQuickMenuEditing = false;
}

void actionBack() {
    if (stackLevel >= 0) {
        currentMenu = menuStack[stackLevel].menu;
        currentMenuSize = menuStack[stackLevel].size;
        cursorPosition = menuStack[stackLevel].cursor;
        viewPosition = menuStack[stackLevel].view;
        stackLevel--;
    } else {
        actionExit();
    }
}

void actionShutdown() {
    executeShutdown();
}

void actionRestore() {
    preferences.clear();
    logStatus("Settings Cleared");
    delay(500);
    ESP.restart();
}

void openPageCreds() { screenSelect = 2; activePageId = 1; }
void openPageLogs() { screenSelect = 2; activePageId = 2; }
void openPageAbout() { screenSelect = 2; activePageId = 3; }

void handleMenuLogic() {
    bool up = getButtonState(0);
    bool down = getButtonState(1);
    bool enter = getButtonState(2);
    bool enterLong = getButtonState(3);

    if (screenSelect == 0) { 
        if (enterLong) {
            statusViewIndex = (statusViewIndex + 1) % 3; 
            return;
        }

        if (enter) {
            MenuItem* item = &quickMenu[quickMenuCursor];
            
            if (item->type == ITEM_ACTION) {
                ((void (*)())item->targetMenuOrFunc)();
            }
            else if (item->type == ITEM_BOOL) {
                changeValue(item, true);
                if (item->persist && item->prefKey) saveSetting(item->prefKey, item->variable, item->type);
                applyPowerSettings();
                
                char buf[20];
                sprintf(buf, "%s: %s", item->name, *(bool*)item->variable ? "ON" : "OFF");
                logStatus(buf);
            }
            else {
                isQuickMenuEditing = !isQuickMenuEditing;
                if (!isQuickMenuEditing) {
                    if (item->persist && item->prefKey) {
                        saveSetting(item->prefKey, item->variable, item->type);
                    }
                    if (item->variable == &qm_dc_vbus || 
                        item->variable == &qm_dc_ibus || 
                        item->variable == &qm_dc_mode_index) {
                        applyPowerSettings();
                        
                        logStatus("DC Params Set");
                    }
                }
            }
        }
        
        if (up || down) {
            if (holdStartTime == 0) holdStartTime = millis();
            bool trigger = (millis() == holdStartTime) || (millis() - holdStartTime > 200 && millis() - lastValueChangeTime > 100);
            
            if (trigger) {
                lastValueChangeTime = millis();
                if (isQuickMenuEditing) {
                    changeValue(&quickMenu[quickMenuCursor], up);
                } else {
                    if (up) quickMenuCursor = (quickMenuCursor == 0) ? 6 : quickMenuCursor - 1;
                    else quickMenuCursor = (quickMenuCursor + 1) % 7;
                }
            }
        } else holdStartTime = 0;
        return;
    }

    // --- SCREEN 1: MAIN MENU ---
    if (screenSelect == 1) { 
        if (enter) {
            MenuItem* item = &currentMenu[cursorPosition];
            
            if (item->type == ITEM_BACK) {
                ((void (*)())item->targetMenuOrFunc)();
            }
            else if (item->type == ITEM_MENU) {
                stackLevel++;
                menuStack[stackLevel] = {currentMenu, currentMenuSize, cursorPosition, viewPosition};
                currentMenu = (MenuItem*)item->targetMenuOrFunc;
                currentMenuSize = item->numOptions;
                cursorPosition = 0;
                viewPosition = 0;
            } 
            else if (item->type == ITEM_ACTION) {
                ((void (*)())item->targetMenuOrFunc)();
            } 
            else if (item->type == ITEM_BOOL) {
                changeValue(item, true);
                if (item->persist && item->prefKey) saveSetting(item->prefKey, item->variable, item->type);
                if (item->variable == &fan_test_startup) { /* Optional: trigger fan logic */ }
            } 
            else {
                isEditing = !isEditing;
                if (!isEditing) {
                    if (item->persist && item->prefKey) {
                        saveSetting(item->prefKey, item->variable, item->type);
                    }
                    if (item->variable == &wifi_mode_index) {
                        setupWiFi(wifi_mode_index);
                    }
                    if (item->variable == &sc_charge_volt_index || item->variable == &sc_ibat_limit) {
                        applySC8812AParams();
                    }
                }
            }
        } else if (enterLong) {
             actionExit();
        }

        if (up || down) {
            if (holdStartTime == 0) holdStartTime = millis();
            bool trigger = (millis() == holdStartTime) || (millis() - holdStartTime > 200 && millis() - lastValueChangeTime > 50);

            if (trigger) {
                lastValueChangeTime = millis();
                if (isEditing) {
                    changeValue(&currentMenu[cursorPosition], up);
                } else {
                    if (up) {
                        if (cursorPosition > 0) {
                            cursorPosition--;
                            if (cursorPosition < viewPosition) viewPosition--;
                        }
                    } else {
                        if (cursorPosition < currentMenuSize - 1) {
                            cursorPosition++;
                            if (cursorPosition >= viewPosition + 5) viewPosition++;
                        }
                    }
                }
            }
        } else holdStartTime = 0;
        return;
    }

    if (screenSelect == 2) { 
        handlePageScroll(up, down, enter);
    }
}