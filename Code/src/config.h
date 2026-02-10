#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

enum ItemType {
    ITEM_MENU,
    ITEM_BACK,
    ITEM_BOOL,
    ITEM_INT,
    ITEM_FLOAT,
    ITEM_STRING,
    ITEM_ACTION,
    ITEM_PAGE
};

struct MenuItem {
    const char* name;
    ItemType type;
    void* variable;
    void* targetMenuOrFunc; 
    float min;
    float max;
    float step;
    const char** options;
    int numOptions;
    bool persist;
    const char* prefKey;
    const void* dynMin; 
    const void* dynMax;
};

struct MenuState {
    MenuItem* menu;
    int size;
    int cursor;
    int view;
};

extern Preferences preferences;

// --- Menu State ---
extern MenuItem quickMenu[]; 
extern int quickMenuCursor;
extern int screenSelect;
extern int statusViewIndex; 
extern int activePageId;
extern MenuItem* currentMenu;
extern int currentMenuSize;
extern int cursorPosition;
extern int viewPosition;
extern bool isEditing;
extern bool isQuickMenuEditing;

// --- Global Settings Variables ---
extern bool qm_usb_out;
extern bool qm_ac_out;
extern int qm_dc_mode_index;
extern float qm_dc_vbus;
extern float qm_dc_ibus;

extern bool apo_enable;
extern int apo_curr_thres;
extern int apo_ac_thres;
extern int apo_delay;

extern int sc_charge_volt_index;
extern float sc_ibat_limit;

extern float mppt_start_volt;
extern float mppt_min_volt;
extern float mppt_max_volt;
extern float mppt_step;
extern float mppt_interval;

extern bool fan_test_startup;
extern int fan_min_pwm;
extern float tbat_min;
extern float tbat_max;
extern float tmod_min;
extern float tmod_max;
extern float tinv_min;
extern float tinv_max;

extern float cal_min_soc_vcel;
extern float cal_max_soc_vcel;
extern float cal_sag_comp;

extern int wifi_mode_index;
extern bool sys_beeper;

// --- [NEW] WiFi & Web Settings ---
extern const char* wifi_sta_ssid;
extern const char* wifi_sta_pass;
extern const char* wifi_ap_ssid;
extern const char* wifi_ap_pass;
extern const char* web_address;

// --- Functions ---
void configSetup();
void loadSettings();
void saveSetting(const char* key, void* val, ItemType type);
void handleMenuLogic();
void logStatus(const char* msg);
String getLogLine(int index);
int getLogCount();

#endif