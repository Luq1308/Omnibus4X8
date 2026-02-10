#ifndef SC8812A_H
#define SC8812A_H

#include <Arduino.h>
#include <Wire.h>

#define SC8812A_I2C_ADDR            0x74

// Register map (only registers used by this library)
#define SC8812A_REG_VBAT_SET        0x00
#define SC8812A_REG_VBUSREF_I_SET   0x01
#define SC8812A_REG_VBUSREF_I_SET2  0x02
#define SC8812A_REG_IBUS_LIM_SET    0x05
#define SC8812A_REG_IBAT_LIM_SET    0x06
#define SC8812A_REG_VINREG_SET      0x07
#define SC8812A_REG_RATIO           0x08
#define SC8812A_REG_CTRL0_SET       0x09
#define SC8812A_REG_CTRL1_SET       0x0A
#define SC8812A_REG_CTRL2_SET       0x0B
#define SC8812A_REG_CTRL3           0x0C
#define SC8812A_REG_VBUS_FB         0x0D
#define SC8812A_REG_VBUS_FB_2       0x0E
#define SC8812A_REG_VBAT_FB         0x0F
#define SC8812A_REG_VBAT_FB_2       0x10
#define SC8812A_REG_IBUS_VAL        0x11
#define SC8812A_REG_IBUS_VAL_2      0x12
#define SC8812A_REG_IBAT_VAL        0x13
#define SC8812A_REG_IBAT_VAL_2      0x14
#define SC8812A_REG_STATUS          0x17
#define SC8812A_REG_MASK            0x19

class SC8812A {
public:
  /**
   * @brief Constructor.
   * @param pstopPin The pin connected to the PSTOP input. -1 if not used.
   */
  SC8812A(int8_t pstopPin = -1);

  /**
   * @brief Initialize the I2C bus (default pins) and the chip.
   */
  bool begin();

  /**
   * @brief Initialize the I2C bus (specific pins) and the chip.
   * @param sda I2C SDA pin.
   * @param scl I2C SCL pin.
   */
  bool begin(int sda, int scl);

  // power control
  /**
   * @brief Enable charging mode (VBUS -> VBAT).
   */
  void enableCharge();

  /**
   * @brief Enable discharge/OTG mode (VBAT -> VBUS).
   */
  void enableDischarge();

  /**
   * @brief Disable the power converter (sets PSTOP high).
   */
  void disablePower();

  // configuration
  /**
   * @brief Set the shunt resistor values for current calculations.
   * @param rs1_mOhm VBUS shunt resistor value in mΩ.
   * @param rs2_mOhm VBAT shunt resistor value in mΩ.
   */
  void setShuntResistors(float rs1_mOhm, float rs2_mOhm); // mΩ

  /**
   * @brief Set the battery cell count (1-4 cells).
   * @param count Cell count code (0=1S, 1=2S, 2=3S, 3=4S).
   */
  void setCellCount(uint8_t count);   // 0..3

  /**
   * @brief Set the target cell termination voltage.
   * @param voltage Datasheet code (000=4.1V, 001=4.2V... 111=4.5V).
   */
  void setCellVoltage(uint8_t voltage); // 0..7 (per-datasheet codes)

  /**
   * @brief Set the bus-side (IBUS) current limit.
   * @param amps Current limit in Amperes.
   */
  void setIBUSCurrentLimit(float amps); // A

  /**
   * @brief Set the battery-side (IBAT) current limit.
   * @param amps Current limit in Amperes.
   */
  void setIBATCurrentLimit(float amps); // A

  /**
   * @brief Set the minimum VBUS voltage (VINREG) for adaptive charging.
   * @param voltage The minimum voltage in Volts.
   */
  void setMinVBUSVoltage(float voltage); // V (VINREG)

  /**
   * @brief Set the target VBUS output voltage (for discharge/OTG mode).
   * @param voltage The target output voltage in Volts.
   */
  void setVBUSVoltage(float voltage); // V for discharging (internal VBUS setting)

  /**
   * @brief Enable or disable VBUS short-circuit current limit foldback.
   * @param enabled true to enable (default), false to disable.
   */
  void enableCurrentFoldback(bool enabled); // enable = foldback active

  /**
   * @brief Enable or disable PFM mode for light loads in discharge.
   * @param enabled true to enable, false for PWM-only (default).
   */
  void enablePFMMode(bool enabled);

  /**
   * @brief Set the converter switching frequency.
   * @param freq 0=150kHz, 1=300kHz, 2=450kHz.
   */
  void setSwitchingFrequency(uint8_t freq); // 0->150k,1->300k,2->450k

  /**
   * @brief Set the switching dead time.
   * @param time 0=20ns, 1=40ns, 2=60ns, 3=80ns.
   */
  void setDeadTime(uint8_t time); // 0..3

  // ADC & telemetry
  /**
   * @brief Enable or disable the ADC for telemetry.
   * @param enabled true to start ADC, false to stop.
   */
  bool enableADC(bool enabled);

  /**
   * @brief Read the VBUS voltage from the ADC.
   * @return VBUS voltage in Volts.
   */
  float getVbusVoltage(); // V

  /**
   * @brief Read the VBAT voltage from the ADC.
   * @return VBAT voltage in Volts.
   */
  float getVbatVoltage(); // V

  /**
   * @brief Read the VBUS current from the ADC.
   * @return VBUS current in Amperes.
   */
  float getIbusCurrent(); // A

  /**
   * @brief Read the VBAT current from the ADC.
   * @return VBAT current in Amperes.
   */
  float getIbatCurrent(); // A

  /**
   * @brief Read the main status register (0x17).
   * @return The 8-bit status register.
   */
  uint8_t getStatus();

private:
  bool _initialize();
  uint8_t readRegister(uint8_t regAddr);
  bool writeRegister(uint8_t regAddr, uint8_t value);
  uint16_t _readRawADC(uint8_t msbAddr); // returns 10-bit raw (0..1023)

  int8_t _pstopPin;

  // calibration
  float _rs1_mOhm; // VBUS shunt in mΩ
  float _rs2_mOhm; // VBAT shunt in mΩ
  float _vbusRatio; // 12.5 or 5.0
  float _vbatRatio; // 12.5 or 5.0
  float _ibusRatio; // 3 or 6
  float _ibatRatio; // 6 or 12
};

#endif // SC8812A_H