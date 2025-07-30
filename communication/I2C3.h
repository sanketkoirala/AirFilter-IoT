#ifndef _I2C3_H_
#define _I2C3_H_
#include <stdint.h>
#include "tm4c123gh6pm.h"

// defining masks for pushbutton, PD0(SCL) and PD1 (SDA)
#define SDA_ 2
#define SCL_ 1

void initI2C3(void);
void writeDatatoRegI2C3(uint8_t, uint8_t, uint8_t);
uint8_t readDataFromRegI2C3(uint8_t add, uint8_t reg);
#endif
