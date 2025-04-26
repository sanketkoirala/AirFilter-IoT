#include "bme680.h"
#include "wait.h"
#include <math.h>

uint8_t bmeSpace[20];
uint8_t bmeDataSpace[20];
uint8_t bmeCalSpace[40];
bmeDataRaw* bmeRaw = (bmeDataRaw*) bmeSpace;
bmeData_s* bmeData = (bmeData_s*) bmeDataSpace;
bmeCalibrationData* bmeCal = (bmeCalibrationData*) bmeCalSpace;

int32_t calculateTemp()
{
    int32_t var1 = ((((bmeRaw->adc_T >> 3) - ((int32_t)bmeCal->par_T1 << 1))) * ((int32_t)bmeCal->par_T2)) >> 11;
    int32_t var2 = (((((bmeRaw->adc_T >> 4) - ((int32_t)bmeCal->par_T1)) * ((bmeRaw->adc_T >> 4) - ((int32_t)bmeCal->par_T1))) >> 12) * ((int32_t)bmeCal->par_T3)) >> 14;
    bmeData->T_fine = var1 + var2;
    return ((bmeData->T_fine * 5 + 128) >> 8); // °C × 100
}

uint32_t calculatePress()
{
    int64_t var1, var2, pressure;

    var1 = ((int64_t)bmeData->T_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmeCal->par_P6;
    var2 = var2 + ((var1 * (int64_t)bmeCal->par_P5) << 17);
    var2 = var2 + (((int64_t)bmeCal->par_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmeCal->par_P3) >> 8) + ((var1 * (int64_t)bmeCal->par_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmeCal->par_P1) >> 33;

    if (var1 == 0)
        return 0;

    pressure = 1048576 - bmeRaw->adc_P;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmeCal->par_P9) * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = (((int64_t)bmeCal->par_P8) * pressure) >> 19;

    pressure = ((pressure + var1 + var2) >> 8) + (((int64_t)bmeCal->par_P7) << 4);

    return (uint32_t)(pressure >> 8);  // return in hPa × 100
}

uint32_t calculateHumidity()
{
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t var6;
    int32_t temp_scaled;
    int32_t calc_hum;

    temp_scaled = (bmeData->T_fine * 5 + 128) >> 8;

    var1 = (int32_t)(bmeRaw->adc_H - ((int32_t)((int32_t)bmeCal->par_H1 * 16)))
                 - (((temp_scaled * (int32_t)bmeCal->par_H3) / ((int32_t)100)) >> 1);

    var2 = ((int32_t)bmeCal->par_H2 * (((temp_scaled * (int32_t)bmeCal->par_H4) / ((int32_t)100))
              + (((temp_scaled * ((temp_scaled * (int32_t)bmeCal->par_H5) / ((int32_t)100))) >> 6) / ((int32_t)100))
              + (int32_t)(1 << 14))) >> 10;

    var3 = var1 * var2;
    var4 = (int32_t)bmeCal->par_H6 << 7;
    var4 = ((var4) + ((temp_scaled * (int32_t)bmeCal->par_H7) / ((int32_t)100))) >> 4;
    var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    var6 = (var4 * var5) >> 1;
    calc_hum = (((var3 + var6) >> 10) * ((int32_t)1000)) >> 12;
    if (calc_hum > 100000)
    {
        calc_hum = 100000;
    }
    else if (calc_hum < 0)
    {
        calc_hum = 0;
    }

    return (uint32_t)calc_hum; // %RH × 1000
}

uint32_t calculateGasResistance()
{
    int64_t var1;
    uint64_t var2;
    int64_t var3;
    uint32_t calc_gas_res;
    uint32_t lookup_table1[16] = {
            (uint32_t)2147483647, (uint32_t)2147483647, (uint32_t)2147483647, (uint32_t)2147483647, (uint32_t)2147483647,
            (uint32_t)2126008810, (uint32_t)2147483647, (uint32_t)2130303777, (uint32_t)2147483647, (uint32_t)2147483647,
            (uint32_t)2143188679, (uint32_t)2136746228, (uint32_t)2147483647, (uint32_t)2126008810, (uint32_t)2147483647,
            (uint32_t)2147483647
    };
    uint32_t lookup_table2[16] = {
             (uint32_t)4096000000, (uint32_t)2048000000, (uint32_t)1024000000, (uint32_t)512000000, (uint32_t)255744255,
             (uint32_t)127110228, (uint32_t)64000000, (uint32_t)32258064, (uint32_t)16016016, (uint32_t)8000000, (uint32_t)4000000,
             (uint32_t)2000000, (uint32_t)1000000, (uint32_t)500000, (uint32_t)250000, (uint32_t)125000
    };

    var1 = (int64_t)((1340 + (5 * (int32_t)bmeCal->range_sw_err)) * ((int64_t)lookup_table1[bmeCal->res_heat_range])) >> 16;
    var2 = (((int64_t)((int64_t)bmeRaw->adc_G << 15) - (int64_t)(16777216)) + var1);
    var3 = (((int64_t)lookup_table2[bmeCal->res_heat_range] * (int64_t)var1) >> 9);
    calc_gas_res = (uint32_t)((var3 + ((int64_t)var2 >> 1)) / (int64_t)var2);

    return calc_gas_res;  // in Ohms
}

float calculateAQI()
{
    uint32_t gas_resistance = bmeData->gasResistance;
    uint32_t gas_baseline = 3500000;  // 3.5 MΩ baseline in good conditions
    float baseline_AQI = 50.0f;
    float max_AQI = 500.0f;            // worst possible AQI

    if (gas_resistance == 0)
        return max_AQI;

    float ratio = (float)gas_baseline / (float)gas_resistance;

    float AQI;

    if (ratio <= 1.0f)
    {
        AQI = baseline_AQI * powf(ratio, 1.5f);
    }
    else
    {
        AQI = baseline_AQI + (max_AQI - baseline_AQI) * (1.0f - powf(1.0f / ratio, 1.5f));
    }

    // Clamp output between 0 and 500
    if (AQI < 0.0f)
        AQI = 0.0f;
    if (AQI > max_AQI)
        AQI = max_AQI;

    return AQI;
}

void startbme()
{
    if(readDataFromRegI2C3(bme_add, id) != 97)
    {
        bmeRaw->dev_on = false;
        return;
    }
    bmeRaw->dev_on = true;
    writeDatatoRegI2C3(bme_add, config, 0x0C); // setting iir filter to 7 00001100
    writeDatatoRegI2C3(bme_add, ctrl_hum, 0x02); // hum 2X oversampling
    //over sampling for temperature, humidity, and pressure
    // |--osrs_t<2:0>--|--osrs_p<2:0>--|--mode<1:0>--|
    //      011             101                 01
    //      4X              16X              forced mode
    //          0x75
    writeDatatoRegI2C3(bme_add, ctrl_meas, 0x75);
    // 01110010 [7:6] represent multiplier (4X), [5:0] are the number being multiplied (50)
    //  50*4 = 200ms  -- 0x72
    writeDatatoRegI2C3(bme_add, gas_wait_0, 0x72);

    bmeData->T_fine = 23;

    uint8_t ctrl = readDataFromRegI2C3(bme_add, ctrl_meas);
    ctrl = (ctrl & 0xFC) | 0x01; // set mode forced again for temp measure
    writeDatatoRegI2C3(bme_add, ctrl_meas, ctrl);
    while(readDataFromRegI2C3(bme_add, eas_status_0) & (1 << 5)); // wait until measuring == 0
    waitMicrosecond(100);
    readBmeCalibrationData();
    readBmeRaw();

    //calculating heater value
    int target_temp = 300;
    float amb_temp = calculateTemp();
    double var1 = ((double)bmeCal->par_G1 / 16.0) + 49.0;
    double var2 = (((double)bmeCal->par_G2 / 32768.0) * 0.0005) + 0.00235;
    double var3 = (double)bmeCal->par_G3 / 1024.0;
    double var4 = var1 * (1.0 + (var2 * (double) target_temp));
    double var5 = var4 + (var3 * (double)amb_temp);
    uint8_t res_heat = (uint8_t)(3.4 * ((var5 * (4.0 / (4.0 + (double)bmeCal->res_heat_range)) * (1.0/(1.0 +
            ((double)bmeCal->res_heat_val * 0.002)))) - 25));


    writeDatatoRegI2C3(bme_add, res_heat_0, res_heat);
    writeDatatoRegI2C3(bme_add, ctrl_gas_1, 0x00); //NB conversion 0
    writeDatatoRegI2C3(bme_add, ctrl_gas_1, 0x10); //enable gass sensor
    readBmeCalibrationData(); // another cal read to make sure gas values are correct -- probably not needed
    ctrl = readDataFromRegI2C3(bme_add, ctrl_meas);
    ctrl = (ctrl & 0xFC) | 0x01; // set mode forced again
    writeDatatoRegI2C3(bme_add, ctrl_meas, ctrl);
    while(readDataFromRegI2C3(bme_add, eas_status_0) & (1 << 5)); // wait until measuring == 0
    return;
}

void readBmeRaw()
{
    if(bmeRaw->dev_on == false)
        return;
    uint8_t i = 0;
    uint8_t statusvar = readDataFromRegI2C3(bme_add, eas_status_0);
    uint8_t gas_lsbtemp = readDataFromRegI2C3(bme_add, gas_r_lsb);
    bool new_data = (statusvar & 0x80) != 0;
    bool gas_valid_0 = (gas_lsbtemp & 0x20) != 0;
    bool heat_stab_0 = (gas_lsbtemp & 0x10) != 0;
    writeDatatoRegI2C3(bme_add, ctrl_gas_1, 0x10);
    uint8_t ctrl = readDataFromRegI2C3(bme_add, ctrl_meas);
    ctrl = (ctrl & 0xFC) | 0x01;  // Set mode = forced
    writeDatatoRegI2C3(bme_add, ctrl_meas, ctrl);

//    char buff[100];
//    snprintf(buff, 50, "new_data: %d\t gas_valid_0: %d\t heat_stab_0: %d\n", new_data, gas_valid_0, heat_stab_0);
//    putsUart0(buff);

    while (readDataFromRegI2C3(bme_add, eas_status_0) & (1 << 5));  // wait while measuring bit is set

    uint8_t data[8];
    for (i = 0; i < 8; i++)
        data[i] = readDataFromRegI2C3(bme_add, press_msb + i);

    uint8_t gas_msb;
    uint8_t gas_lsb;
    if(new_data && gas_valid_0 && heat_stab_0)
    {
        // Read gas data from 0x2A and 0x2B
        gas_msb = readDataFromRegI2C3(bme_add, 0x2A);
        gas_lsb = readDataFromRegI2C3(bme_add, 0x2B);
    }
    // raw values
    bmeRaw->adc_P = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | ((data[2] >> 4) & 0x0F);
    bmeRaw->adc_T = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | ((data[5] >> 4) & 0x0F);
    bmeRaw->adc_H = ((uint32_t)data[6] << 8) | data[7];

    bmeRaw->adc_G = ((uint16_t)gas_msb << 2) | ((gas_lsb & 0xC0) >> 6);
    bmeRaw->gas_range = gas_lsb & 0x0F;
    return;
}

void updateBmeData()
{
    if(bmeRaw->dev_on == false)
            return;
    readBmeRaw();
    bmeData->temperature = calculateTemp();
    bmeData->pressure = calculatePress();
    bmeData->humidity = calculateHumidity();
    bmeData->gasResistance = calculateGasResistance();
    bmeData->AQI = calculateAQI();
    return;
}

void readBmeCalibrationData()
{
    if(bmeRaw->dev_on == false)
            return;
    //temp calibration data
    bmeCal->par_T1 = readDataFromRegI2C3(bme_add, 0xE9);
    bmeCal->par_T1 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0xEA)) << 8;
    uint16_t T2_lsb = readDataFromRegI2C3(bme_add, 0x8A);
    uint16_t T2_msb = readDataFromRegI2C3(bme_add, 0x8B);
    bmeCal->par_T2 = (int16_t)((T2_msb << 8) | T2_lsb);
    bmeCal->par_T3 = (int8_t)readDataFromRegI2C3(bme_add, 0x8C);

    //press calibration data
    bmeCal->par_P1 = readDataFromRegI2C3(bme_add, 0x8E);
    bmeCal->par_P1 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x8F)) << 8;
    bmeCal->par_P2 = readDataFromRegI2C3(bme_add, 0x90);
    bmeCal->par_P2 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x91)) << 8;
    bmeCal->par_P2 = (int16_t) bmeCal->par_P2;
    bmeCal->par_P3 = (int8_t)readDataFromRegI2C3(bme_add, 0x92);
    bmeCal->par_P4 = readDataFromRegI2C3(bme_add, 0x94);
    bmeCal->par_P4 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x95)) << 8;
    bmeCal->par_P4 = (int16_t) bmeCal->par_P4;
    bmeCal->par_P5 = readDataFromRegI2C3(bme_add, 0x96);
    bmeCal->par_P5 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x97)) << 8;
    bmeCal->par_P5 = (int16_t) bmeCal->par_P5;
    bmeCal->par_P6 = (int8_t)readDataFromRegI2C3(bme_add, 0x99);
    bmeCal->par_P7 = (int8_t)readDataFromRegI2C3(bme_add, 0x98);
    bmeCal->par_P8 = readDataFromRegI2C3(bme_add, 0x9C);
    bmeCal->par_P8 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x9D)) << 8;
    bmeCal->par_P8 = (int16_t) bmeCal->par_P8;
    bmeCal->par_P9 = readDataFromRegI2C3(bme_add, 0x9E);
    bmeCal->par_P9 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0x9F)) << 8;
    bmeCal->par_P9 = (int16_t) bmeCal->par_P9;
    bmeCal->par_P10 = readDataFromRegI2C3(bme_add, 0xA0);

    //humidity cal data
    bmeCal->par_H1 = readDataFromRegI2C3(bme_add, 0xE2) & 0x0F;
    bmeCal->par_H1 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0xE3)) << 4;
    bmeCal->par_H2 = (readDataFromRegI2C3(bme_add, 0xE2) & 0xF0) >> 4;
    bmeCal->par_H2 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0xE1)) << 4;
    bmeCal->par_H3 = (int8_t)readDataFromRegI2C3(bme_add, 0xE4);
    bmeCal->par_H4 = (int8_t)readDataFromRegI2C3(bme_add, 0xE5);
    bmeCal->par_H5 = (int8_t)readDataFromRegI2C3(bme_add, 0xE6);
    bmeCal->par_H6 = readDataFromRegI2C3(bme_add, 0xE7);
    bmeCal->par_H7 = (int8_t)readDataFromRegI2C3(bme_add, 0xE8);

    //gas cal data
    bmeCal->par_G1 = (int8_t)readDataFromRegI2C3(bme_add, 0xED);
    bmeCal->par_G2 = readDataFromRegI2C3(bme_add, 0xEB);
    bmeCal->par_G2 |= ((uint16_t)readDataFromRegI2C3(bme_add, 0xEC)) << 8;
    bmeCal->par_G2 = (int16_t) bmeCal->par_G2;
    bmeCal->par_G3 = (int8_t)readDataFromRegI2C3(bme_add, 0xEE);
    bmeCal->res_heat_range = (readDataFromRegI2C3(bme_add, 0x02) & 0xF0) >> 4;  // gets bits 7:4, but may include other info
    bmeCal->res_heat_range &= 0x03;  // isolate just the 2 bits from 5:4
    bmeCal->res_heat_val = (int8_t)readDataFromRegI2C3(bme_add, 0x00);
    bmeCal->range_sw_err = (int8_t)readDataFromRegI2C3(bme_add, 0x04);
    return;
}

void printData()
{
    if(bmeRaw->dev_on == false)
            return;
    char buff[100];
    snprintf(buff, 100, "Raw Data Read-out:\nadc_T: %d\t adc_P: %d\t adc_H: %d\t adc_G: %d\t gas_range: %d\n",
             bmeRaw->adc_T, bmeRaw->adc_P, bmeRaw->adc_H, bmeRaw->adc_G, bmeRaw->gas_range);
    putsUart0(buff);
    snprintf(buff, 100, "par_G1: %d\t par_G2: %d\t par_G3: %d\t res_heat_range: %d\t res_heat_val: %d\t sw_err: %d\n",
                bmeCal->par_G1, bmeCal->par_G2, bmeCal->par_G3, bmeCal->res_heat_range, bmeCal->res_heat_val, bmeCal->range_sw_err);
    putsUart0(buff);
    snprintf(buff, 100, "Temp: %.3f\t Press: %.3f\t hum: %.3f\t gas: %d\t AQI: %.3f\n",
            (float)bmeData->temperature/100, (float)bmeData->pressure/100, (float)bmeData->humidity/1000, bmeData->gasResistance, bmeData->AQI);
    putsUart0(buff);
    return;
}

bmeDataRaw* getBmeDataRaw()
{
    return bmeRaw;
}

bmeData_s* getBmeData()
{
    return bmeData;
}

bmeCalibrationData* getBmeCalData()
{
    return bmeCal;
}
