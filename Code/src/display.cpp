#include "display.h"
#include "system.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

extern int pageScrollY; 

// --- Helper Functions  ---

void drawLabelAndValue(int x, int y, int width, const char* label, const char* valueStr) {
    const int textPadding = 3;
    u8g2.drawStr(x + textPadding, y, label);
    int valueX = (x + width) - u8g2.getStrWidth(valueStr) - textPadding;
    u8g2.drawStr(valueX, y, valueStr);
}

void drawBatteryIndicator(int x, int y, int width, int height, float soc_percent) {
    u8g2.drawRFrame(x, y, width, height, 3);
    int terminalWidth = width / 24;
    int terminalHeight = height / 3;
    u8g2.drawBox(x + width, y + (height / 2) - (terminalHeight / 2), terminalWidth, terminalHeight);
    
    soc_percent = constrain(soc_percent, 0.0f, 100.0f);
    int maxFill = width - 4; 
    int fillWidth = map(soc_percent, 0, 100, 0, maxFill);
    if (soc_percent > 0.0f) fillWidth = max(fillWidth, 4);
    fillWidth = min(fillWidth, maxFill);
    
    if (soc_percent > 0.0f) u8g2.drawRBox(x + 2, y + 2, fillWidth, height - 4, 2);
}

void drawTelemetryPanel(int x, int y, int width, const char* labels[], float* values, const char* formats[], int count) {
    const int ySpacing = 8;
    const int topPadding = 9;
    int height = (count * ySpacing) + 4;

    u8g2.drawFrame(x, y, width, height);

    char valueBuffer[20];
    for (int i = 0; i < count; i++) {
        int yPos = y + topPadding + (i * ySpacing);
        sprintf(valueBuffer, formats[i], values[i]);
        drawLabelAndValue(x, yPos, width, labels[i], valueBuffer);
    }
}

// --- Component Drawers ---

void drawQuickMenu() {
    const int frameX = 0, frameY = 0, frameWidth = 55;
    const int textPadding = 3;
    const int cursorWidth = 6;
    const int QUICK_MENU_Y_SPACING = 8;

    int frameHeight = (7 * QUICK_MENU_Y_SPACING) + textPadding + 1; // 7 items
    u8g2.drawFrame(frameX, frameY, frameWidth, frameHeight);

    for (int i = 0; i < 7; i++) {
        MenuItem* item = &quickMenu[i];
        int yPos = frameY + 1 + (i * QUICK_MENU_Y_SPACING) + QUICK_MENU_Y_SPACING;

        bool showCursor = !isQuickMenuEditing || (i != quickMenuCursor) || (millis() % 800) > 400;
        if (i == quickMenuCursor && showCursor) {
            u8g2.drawStr(2, yPos, ">");
        }

        u8g2.drawStr(frameX + textPadding + cursorWidth - 1, yPos, item->name);

        if (item->type != ITEM_ACTION) {
            char valueStr[16] = "";
            if (item->type == ITEM_BOOL) sprintf(valueStr, "%s", *(bool*)item->variable ? "ON" : "OFF");
            else if (item->type == ITEM_FLOAT) dtostrf(*(float*)item->variable, 3, 1, valueStr);
            else if (item->type == ITEM_STRING) strncpy(valueStr, item->options[*(int*)item->variable], 15);
            
            int valueX = (frameX + frameWidth) - textPadding - u8g2.getStrWidth(valueStr);
            u8g2.drawStr(valueX, yPos, valueStr);
        }
    }
}

// --- Main Screens ---

void drawStatusScreen() {
    u8g2.clearBuffer();
    drawQuickMenu();

    const int PANEL_X = 57;
    const int PANEL_WIDTH = 60;

    if (statusViewIndex == 0) { // Main Battery Info
        const char* labels[] = {"VBAT", "IBAT", "PBAT", "VCEL", "SOC"};
        float vals[] = {vbat_read, ibat_read, pbat_read, vcel_read, soc};
        const char* fmts[] = {"%.2fV", "%.2fA", "%.0fW", "%.2fV", "%.0f%%"};
        drawTelemetryPanel(PANEL_X, 0, PANEL_WIDTH, labels, vals, fmts, 5);
        drawBatteryIndicator(59, 48, 52, 12, soc);
    } 
    else if (statusViewIndex == 1) { // Power View
        const char* l1[] = {"VBAT", "IBAT", "PBAT"};
        float v1[] = {vbat_read, ibat_read, pbat_read};
        const char* f1[] = {"%.2fV", "%.2fA", "%.0fW"};
        drawTelemetryPanel(PANEL_X, 0, PANEL_WIDTH, l1, v1, f1, 3);
        
        const char* l2[] = {"VBUS", "IBUS", "PBUS"};
        float v2[] = {vbus_read, ibus_read, pbus_read};
        const char* f2[] = {"%.2fV", "%.2fA", "%.0fW"};
        drawTelemetryPanel(PANEL_X, 30, PANEL_WIDTH, l2, v2, f2, 3);
    }
    else if (statusViewIndex == 2) { // Temp View
        float fSpeedPercent = fanSpeed * 100.0;
        const char* labels[] = {"TBAT", "TTMD", "TBMD", "TINV", "FAN"};
        float vals[] = {tempReadings[0], tempReadings[1], tempReadings[2], tempReadings[3], fSpeedPercent};
        const char* fmts[] = {"%.1fC", "%.1fC", "%.1fC", "%.1fC", "%.0f%%"};
        drawTelemetryPanel(PANEL_X, 0, PANEL_WIDTH, labels, vals, fmts, 5);
        drawBatteryIndicator(59, 48, 52, 12, soc);
    }

    if (apo_enable) {
        if (apoCountingDown) {
            if ((millis() % 1000) > 500) {
                u8g2.drawRBox(124, 60, 4, 4, 1);
            }
        } else {
            u8g2.drawRBox(124, 60, 4, 4, 1);
        }
    }

    u8g2.sendBuffer();
}

void drawMenu() {
    u8g2.clearBuffer();

    const int VISIBLE_ROWS = 5;
    const int ROW_HEIGHT = 12;

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int itemIndex = viewPosition + i;
        if (itemIndex >= currentMenuSize) break;
        int yPos = (i * ROW_HEIGHT) + ROW_HEIGHT - 2;
        
        u8g2.drawStr(2, yPos, currentMenu[itemIndex].name);
        
        char valueBuffer[16];
        MenuItem* item = &currentMenu[itemIndex];
        
        switch (item->type) {
            case ITEM_INT:    sprintf(valueBuffer, "%d", *(int*)item->variable); break;
            case ITEM_FLOAT:  dtostrf(*(float*)item->variable, 4, (item->step < 1.0)? 2 : 1, valueBuffer); break;
            case ITEM_BOOL:   sprintf(valueBuffer, "%s", *(bool*)item->variable ? "On" : "Off"); break;
            case ITEM_STRING: strncpy(valueBuffer, item->options[*(int*)item->variable], 15); break;
            default: valueBuffer[0] = '\0'; break;
        }
        
        if (valueBuffer[0] != '\0') {
            u8g2.drawStr(127 - u8g2.getStrWidth(valueBuffer), yPos, valueBuffer);
        }
    }

    int cursorY = (cursorPosition - viewPosition) * ROW_HEIGHT;
    bool showCursor = !isEditing || (millis() % 800) > 400;
    if (showCursor) {
        u8g2.drawHLine(0, cursorY + 1, 128);
        u8g2.drawHLine(0, cursorY + ROW_HEIGHT, 128);
    }
    u8g2.sendBuffer();
}

void drawPage() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont10_tf);
    int maxLines = 6;
    
    if (activePageId == 1) { 
        u8g2.drawStr(0, 10, "   --- Credentials ---");
        char lines[7][30];
        
        sprintf(lines[0], "Web: %s", web_address);
        sprintf(lines[1], "STA SSID: %s", wifi_sta_ssid);
        sprintf(lines[2], "STA Pass: %s", wifi_sta_pass); 
        sprintf(lines[3], "STA IP: %s", WiFi.localIP().toString().c_str());
        
        sprintf(lines[4], "AP SSID: %s", wifi_ap_ssid);
        sprintf(lines[5], "AP Pass: %s", wifi_ap_pass);
        sprintf(lines[6], "AP IP: %s", WiFi.softAPIP().toString().c_str());
        
        int count = 7;
        for (int i=0; i<maxLines; i++) {
            int idx = i + pageScrollY;
            if (idx < count) {
                u8g2.setCursor(0, 20 + (i*10));
                u8g2.print(lines[idx]);
            }
        }
    } else if (activePageId == 2) { 
        u8g2.drawStr(0, 10, "   --- Status Logs ---");
        int count = getLogCount();
        for (int i=0; i<maxLines; i++) {
            int idx = i + pageScrollY;
            if (idx < count) {
                u8g2.setCursor(0, 20 + (i*10));
                u8g2.print(getLogLine(count - 1 - idx)); 
            }
        }
    } else if (activePageId == 3) {
        u8g2.drawStr(0, 10, "      --- About ---");
        u8g2.drawStr(0, 20, "Omnibus 4X8 Power Bank");
        u8g2.drawStr(0, 30, "HW 1.0");
        u8g2.drawStr(0, 40, "FW 1.0.0");
    }
    
    u8g2.sendBuffer();
}

void displaySetup() {
    u8g2.begin();
    u8g2.setFont(u8g2_font_profont10_tf);
}