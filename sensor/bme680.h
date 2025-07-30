#ifndef BME680_H_
#define BME680_H_

#include <stdint.h>
#include <stdio.h>
#include "I2C3.h"
#include "uart0.h"
//defining addresses
#define bme_add 0x77
#define id 0xD0
#define config 0x75
#define ctrl_meas 0x74
#define status 0x73
#define ctrl_hum 0x72
#define ctrl_gas_1 0x71
#define gas_wait_0 0x6D
#define res_heat_0 0x63
#define gas_r_lsb 0x2B
#define gas_r_msb 0x2A
#define hum_lsb 0x26
#define hum_msb 0x25
#define temp_xlsb 0x24
#define temp_lsb 0x23
#define temp_msb 0x22
#define press_xlsb 0x21
#define press_lsb 0x20
#define press_msb 0x1F

#define eas_status_0 0x1D

typedef struct
{
    float temperature;
    float pressure;
    float humidity;
    uint32_t gasResistance;
    float AQI;
    int32_t T_fine;
}bmeData_s;

typedef struct
{
    uint32_t adc_T; // temp
    uint32_t adc_P; // press
    uint32_t adc_H; // humidity
    uint16_t adc_G; // gas
    uint8_t gas_range;
    bool dev_on;
}bmeDataRaw;

typedef struct {
    // Temperature
    uint16_t par_T1;
    int16_t  par_T2;
    int8_t   par_T3;

    // Pressure
    uint16_t par_P1;
    int16_t  par_P2;
    int8_t   par_P3;
    int16_t  par_P4;
    int16_t  par_P5;
    int8_t   par_P6;
    int8_t   par_P7;
    int16_t  par_P8;
    int16_t  par_P9;
    uint8_t  par_P10;

    // Humidity
    uint16_t par_H1;
    uint16_t par_H2;
    int8_t   par_H3;
    int8_t   par_H4;
    int8_t   par_H5;
    uint8_t  par_H6;
    int8_t   par_H7;

    // Gas
    int8_t   par_G1;
    int16_t  par_G2;
    int8_t   par_G3;

    // Other
    uint8_t res_heat_range;
    int8_t  res_heat_val;
    int8_t  range_sw_err;
} bmeCalibrationData;

extern bmeDataRaw* bmeRaw;
extern bmeData_s* bmeData;
extern bmeCalibrationData* bmeCal;

int32_t calculateTemp();
uint32_t calculatePress();
uint32_t calculateHumidity();
uint32_t calculateGasResistance();
void updateBmeData();
void startbme();
void readBmeRaw();
void readBmeCalibrationData();
void printData();
bmeDataRaw* getBmeDataRaw();
bmeData_s* getBmeData();
bmeCalibrationData* getBmeCalData();

#endif
