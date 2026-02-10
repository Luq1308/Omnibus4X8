#include "SC8812A.h"

/*
 Minimal implementation / docs.
 All register math follows the SC8812A datasheet register map and formulas.
 See datasheet register tables for formulas used here. (VBUSREF, IBUS/IBAT formulas, VINREG).
*/

SC8812A::SC8812A(int8_t pstopPin)
  : _pstopPin(pstopPin)
{
  // defaults (match typical values / datasheet POR)
  _rs1_mOhm = 10.0f;  // common sense default (user should call setShuntResistors)
  _rs2_mOhm = 10.0f;
  _vbusRatio = 12.5f; // default VBUS_RATIO = 0 -> 12.5x
  _vbatRatio = 12.5f; // VBAT_MON_RATIO default 0 -> 12.5x
  _ibusRatio = 3.0f;  // default IBUS_RATIO = 10b -> 3x (per datasheet default)
  _ibatRatio = 12.0f; // default IBAT_RATIO = 1 -> 12x
}

bool SC8812A::begin() {
  Wire.begin();
  return _initialize();
}

bool SC8812A::begin(int sda, int scl) {
  Wire.begin(sda, scl);
  return _initialize();
}

// --- power control ---
void SC8812A::enableCharge() {
  enableADC(true);
  if (_pstopPin != -1) digitalWrite(_pstopPin, LOW);

  uint8_t regVal = readRegister(SC8812A_REG_CTRL0_SET);
  if (regVal == 0xFF) return;
  regVal &= ~(1 << 7); // EN_OTG = 0 -> charging
  writeRegister(SC8812A_REG_CTRL0_SET, regVal);
}

void SC8812A::enableDischarge() {
  enableADC(true);
  if (_pstopPin != -1) digitalWrite(_pstopPin, LOW);

  uint8_t regVal = readRegister(SC8812A_REG_CTRL0_SET);
  if (regVal == 0xFF) return;
  regVal |= (1 << 7); // EN_OTG = 1 -> discharging (OTG)
  writeRegister(SC8812A_REG_CTRL0_SET, regVal);
}

void SC8812A::disablePower() {
  if (_pstopPin != -1) digitalWrite(_pstopPin, HIGH);
  uint8_t regVal = readRegister(SC8812A_REG_CTRL0_SET);
  if (regVal == 0xFF) return;
  regVal &= ~(1 << 7); // ensure EN_OTG cleared
  writeRegister(SC8812A_REG_CTRL0_SET, regVal);
}

// --- configuration ---
void SC8812A::setShuntResistors(float rs1_mOhm, float rs2_mOhm) {
  if (rs1_mOhm > 0.0f) _rs1_mOhm = rs1_mOhm;
  if (rs2_mOhm > 0.0f) _rs2_mOhm = rs2_mOhm;
}

void SC8812A::setCellCount(uint8_t count) {
  uint8_t regVal = readRegister(SC8812A_REG_VBAT_SET);
  if (regVal == 0xFF) return;
  regVal &= ~(0b11 << 3); // CSEL bits at [4:3]
  regVal |= ((count & 0b11) << 3); 
  writeRegister(SC8812A_REG_VBAT_SET, regVal);
}

void SC8812A::setCellVoltage(uint8_t voltage) {
  uint8_t regVal = readRegister(SC8812A_REG_VBAT_SET);
  if (regVal == 0xFF) return;
  regVal &= ~0b111; // VCELL_SET bits [2:0]
  regVal |= (voltage & 0b111);
  writeRegister(SC8812A_REG_VBAT_SET, regVal);
}

void SC8812A::setIBUSCurrentLimit(float amps) {
  if (amps < 0.3f) amps = 0.3f; // datasheet minimum suggestion

  // Datasheet formula:
  // IBUS_LIM (A) = (IBUS_LIM_SET + 1) / 256 × IBUS_RATIO × 10mΩ / RS1
  // Rearranged:
  // IBUS_LIM_SET = IBUS_A * 256 * RS1 / (IBUS_RATIO * 10mΩ) - 1
  float denom_mOhm = (_ibusRatio * 10.0f); // IBUS_RATIO * 10 mΩ
  float setf = (amps * 256.0f * _rs1_mOhm) / denom_mOhm - 1.0f;
  int val = (int)round(constrain(setf, 0.0f, 255.0f));
  writeRegister(SC8812A_REG_IBUS_LIM_SET, (uint8_t)val);
}

void SC8812A::setIBATCurrentLimit(float amps) {
  if (amps < 0.3f) amps = 0.3f; // datasheet minimum suggestion

  // Datasheet formula:
  // IBAT_LIM (A) = (IBAT_LIM_SET + 1) / 256 × IBAT_RATIO × 10mΩ / RS2
  // Rearranged:
  // IBAT_LIM_SET = IBAT_A * 256 * RS2 / (IBAT_RATIO * 10mΩ) - 1
  float denom_mOhm = (_ibatRatio * 10.0f); // IBAT_RATIO * 10 mΩ
  float setf = (amps * 256.0f * _rs2_mOhm) / denom_mOhm - 1.0f;
  int val = (int)round(constrain(setf, 0.0f, 255.0f));
  writeRegister(SC8812A_REG_IBAT_LIM_SET, (uint8_t)val);
}

void SC8812A::setMinVBUSVoltage(float voltage) {
  // VINREG selection: choose ratio 40x when VINREG target < 10.24V per datasheet
  bool ratio_bit = (voltage <= 10.24f);
  float ratio_val = ratio_bit ? 40.0f : 100.0f; // factor (mV multiplier)
  uint8_t ctrl0 = readRegister(SC8812A_REG_CTRL0_SET);
  if (ctrl0 == 0xFF) return;
  if (ratio_bit) ctrl0 |= (1 << 4); // VINREG_RATIO bit 4
  else ctrl0 &= ~(1 << 4);
  writeRegister(SC8812A_REG_CTRL0_SET, ctrl0);

  // VINREG formula per datasheet:
  // VINREG = (VINREG_SET + 1) × VINREG_RATIO (mV)
  float reg_val = ((voltage * 1000.0f) / ratio_val) - 1.0f;
  uint8_t set = (uint8_t)constrain((int)round(reg_val), 0, 255);
  writeRegister(SC8812A_REG_VINREG_SET, set);
}

void SC8812A::setVBUSVoltage(float voltage) {
  // Using internal VBUS reference (FB_SEL = 0). Per datasheet:
  // VBUSREF_I = (4 * VBUSREF_I_SET + VBUSREF_I_SET2 + 1) x 2 mV
  // VBUS = VBUSREF_I x VBUS_RATIO
  float vbus_ref_mv = (voltage * 1000.0f) / _vbusRatio;
  int32_t raw_10bit = (int32_t)(vbus_ref_mv / 2.0f) - 1; // (4*SET + SET2) = VBUSREF_I_mV/2 - 1
  raw_10bit = constrain(raw_10bit, 0, 1023);

  uint8_t msb = (uint8_t)(raw_10bit >> 2); // highest 8 bits
  uint8_t lsb2 = (uint8_t)(raw_10bit & 0x03); // lowest 2 bits

  writeRegister(SC8812A_REG_VBUSREF_I_SET, msb);

  uint8_t reg02 = readRegister(SC8812A_REG_VBUSREF_I_SET2);
  if (reg02 == 0xFF) return;
  reg02 &= ~(0b11 << 6); // VBUSREF_I_SET2 bits [7:6]
  reg02 |= (lsb2 << 6);
  writeRegister(SC8812A_REG_VBUSREF_I_SET2, reg02);
}

void SC8812A::enableCurrentFoldback(bool enabled) {
  uint8_t reg = readRegister(SC8812A_REG_CTRL3);
  if (reg == 0xFF) return;
  // Datasheet: DIS_ShortFoldBack bit (bit2) -> 0 = foldback enabled, 1 = disable foldback
  if (enabled) reg &= ~(1 << 2);
  else reg |= (1 << 2);
  writeRegister(SC8812A_REG_CTRL3, reg);
}

void SC8812A::enablePFMMode(bool enabled) {
  uint8_t reg = readRegister(SC8812A_REG_CTRL3);
  if (reg == 0xFF) return;
  if (enabled) reg |= (1 << 0); // EN_PFM is bit0 per datasheet
  else reg &= ~(1 << 0);
  writeRegister(SC8812A_REG_CTRL3, reg);
}

void SC8812A::setSwitchingFrequency(uint8_t freq) {
  // mapping: 0->00(150k), 1->01(300k), 2->11(450k)
  uint8_t bits;
  if (freq == 0) bits = 0b00;
  else if (freq == 1) bits = 0b01;
  else bits = 0b11;
  uint8_t reg = readRegister(SC8812A_REG_CTRL0_SET);
  if (reg == 0xFF) return;
  reg &= ~(0b11 << 2); // FREQ_SET bits [3:2]
  reg |= (bits << 2);
  writeRegister(SC8812A_REG_CTRL0_SET, reg);
}

void SC8812A::setDeadTime(uint8_t time) {
  uint8_t reg = readRegister(SC8812A_REG_CTRL0_SET);
  if (reg == 0xFF) return;
  reg &= ~0b11; // DT_SET bits [1:0]
  reg |= (time & 0b11);
  writeRegister(SC8812A_REG_CTRL0_SET, reg);
}

// --- ADC & telemetry ---
bool SC8812A::enableADC(bool enabled) {
  uint8_t reg = readRegister(SC8812A_REG_CTRL3);
  if (reg == 0xFF) return false;
  // AD_START is bit 5 per datasheet (1=start ADC)
  if (enabled) reg |= (1 << 5);
  else reg &= ~(1 << 5);
  writeRegister(SC8812A_REG_CTRL3, reg);
  return true;
}

float SC8812A::getVbusVoltage() {
  uint16_t raw = _readRawADC(SC8812A_REG_VBUS_FB);
  // Datasheet: VBUS = (4*VBUS_FB_VALUE + VBUS_FB_VALUE2 + 1) x VBUS_RATIO x 2 mV
  return (float)(raw + 1) * _vbusRatio * 0.002f;
}

float SC8812A::getVbatVoltage() {
  uint16_t raw = _readRawADC(SC8812A_REG_VBAT_FB);
  // Datasheet: VBAT = (4 x VBAT_FB_VALUE + VBAT_FB_VALUE2 + 1) x VBAT_MON_RATIO x 2 mV
  return (float)(raw + 1) * _vbatRatio * 0.002f;
}

float SC8812A::getIbusCurrent() {
  uint16_t raw = _readRawADC(SC8812A_REG_IBUS_VAL);
  // Datasheet: IBUS (A) = (raw+1) * 2 / 1200 * IBUS_RATIO * 10mΩ / RS1
  float ibus = ((float)(raw + 1) * 2.0f / 1200.0f) * _ibusRatio * (10.0f / _rs1_mOhm);
  return ibus;
}

float SC8812A::getIbatCurrent() {
  uint16_t raw = _readRawADC(SC8812A_REG_IBAT_VAL);
  // Datasheet: IBAT (A) = (raw+1) * 2 / 1200 * IBAT_RATIO * 10mΩ / RS2
  float ibat = ((float)(raw + 1) * 2.0f / 1200.0f) * _ibatRatio * (10.0f / _rs2_mOhm);
  return ibat;
}

uint8_t SC8812A::getStatus() {
  return readRegister(SC8812A_REG_STATUS);
}

// --- private utilities ---
bool SC8812A::_initialize() {
  if (_pstopPin != -1) {
    pinMode(_pstopPin, OUTPUT);
    digitalWrite(_pstopPin, HIGH); // standby
  }

  // quick I2C check
  Wire.beginTransmission(SC8812A_I2C_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // set FACTORY bit (datasheet recommends MCU write this bit to 1 after power up)
  uint8_t ctrl2 = readRegister(SC8812A_REG_CTRL2_SET);
  if (ctrl2 == 0xFF) return false;
  ctrl2 |= (1 << 3); // FACTORY bit is bit3
  writeRegister(SC8812A_REG_CTRL2_SET, ctrl2);

  return true;
}

uint8_t SC8812A::readRegister(uint8_t addr) {
  Wire.beginTransmission(SC8812A_I2C_ADDR);
  Wire.write(addr);
  if (Wire.endTransmission() != 0) return 0xFF;

  // request one byte; Wire.requestFrom returns the number of bytes received
  uint32_t t0 = millis();
  Wire.requestFrom((uint8_t)SC8812A_I2C_ADDR, (uint8_t)1);
  while (Wire.available() == 0) {
    if ((millis() - t0) > 5) return 0xFF; // short timeout
  }
  return Wire.read();
}

bool SC8812A::writeRegister(uint8_t addr, uint8_t val) {
  Wire.beginTransmission(SC8812A_I2C_ADDR);
  Wire.write(addr);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

uint16_t SC8812A::_readRawADC(uint8_t msbAddr) {
  // read MSB register and the following LSB register (msbAddr and msbAddr+1)
  Wire.beginTransmission(SC8812A_I2C_ADDR);
  Wire.write(msbAddr);
  if (Wire.endTransmission() != 0) return 0; // I2C error

  // request two bytes
  uint32_t t0 = millis();
  Wire.requestFrom((uint8_t)SC8812A_I2C_ADDR, (uint8_t)2);
  while (Wire.available() < 2) {
    if ((millis() - t0) > 5) {
      // timeout, return 0 as safe fallback
      return 0;
    }
  }
  uint8_t msb = Wire.read(); // highest 8 bits
  uint8_t lsb = Wire.read(); // contains lowest 2 bits in its [7:6]
  // assemble 10-bit value: (MSB << 2) | (LSB >> 6)
  uint16_t raw10 = ((uint16_t)msb << 2) | ((lsb >> 6) & 0x03);
  return raw10;
}