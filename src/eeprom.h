#ifndef EEPROM_H
#define EEPROM_H

#include "stm32f0xx.h"

// EEPROM I2C Address (7-bit)
#define EEPROM_ADDR 0x50

// Memory address for storing high score
#define HIGH_SCORE_ADDR 0x0000

// Function prototypes
void eeprom_init(void);

// Basic EEPROM operations
int8_t eeprom_write_byte(uint16_t addr, uint8_t data);
int8_t eeprom_read_byte(uint16_t addr, uint8_t *data);

// High score specific functions
int8_t eeprom_save_high_score(uint32_t score);
int8_t eeprom_get_high_score(uint32_t *score);

// Status codes
#define EEPROM_OK     0
#define EEPROM_ERROR -1

// Timing constants (in milliseconds)
#define EEPROM_WRITE_CYCLE_TIME 5

#endif /* EEPROM_H */

