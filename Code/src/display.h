#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

void displaySetup();
void drawStatusScreen();
void drawMenu();
void drawPage();

#endif