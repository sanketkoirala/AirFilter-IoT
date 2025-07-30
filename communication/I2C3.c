#include "I2C3.h"
#include "../clock/clock.h"

void initI2C3(void)
{
    // enable clocks for I2C and GPIO port f and port a
    SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R3;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3 | SYSCTL_RCGCGPIO_R1;
    _delay_cycles(3);

    GPIO_PORTD_DEN_R |= SDA_ | SCL_;
    GPIO_PORTD_DIR_R |= SDA_ | SCL_;

    // configure PD0 and PD1 for alt function
    GPIO_PORTD_AFSEL_R |= SDA_ | SCL_;
    GPIO_PORTD_PUR_R |= SDA_ | SCL_;
    GPIO_PORTD_ODR_R |= SDA_;
    GPIO_PORTD_ODR_R &= ~SCL_;
    GPIO_PORTD_PCTL_R &= ~(GPIO_PCTL_PB2_M | GPIO_PCTL_PB3_M);
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD1_I2C3SDA | GPIO_PCTL_PD0_I2C3SCL;

    // initialize I2C as master
    I2C3_MCR_R = 0;
    I2C3_MCR_R = I2C_MCR_MFE;
    // calculation for SCL clock speed of 400 kbps
    //  I2CMTPR = (40 MHz/ (2*(6+4) * 400000))-1 = 4
    I2C3_MTPR_R = 4;
    I2C3_MCS_R = I2C_MCS_STOP; // initialize in STOP condition
}

// blocking
void writeDatatoRegI2C3(uint8_t add, uint8_t reg, uint8_t data)
{
    I2C3_MSA_R = add << 1 | 0; // 0 for write operation
    I2C3_MDR_R = reg;
    I2C3_MICR_R = I2C_MICR_IC;
    I2C3_MCS_R = I2C_MCS_START | I2C_MCS_RUN;
    while ((I2C3_MRIS_R & I2C_MRIS_RIS) == 0)
        ;
    // write data to device
    I2C3_MDR_R = data;
    I2C3_MICR_R = I2C_MICR_IC;
    //    I2C0_MCS_R = ~I2C_MCS_ACK;
    I2C3_MCS_R = I2C_MCS_RUN | I2C_MCS_STOP;
    while ((I2C3_MRIS_R & I2C_MRIS_RIS) == 0)
        ;
}

uint8_t readDataFromRegI2C3(uint8_t add, uint8_t reg)
{
    I2C3_MSA_R = add << 1 | 0; // setting internal counter for device
    I2C3_MDR_R = reg;
    I2C3_MICR_R = I2C_MICR_IC;
    I2C3_MCS_R = I2C_MCS_START | I2C_MCS_RUN;
    while ((I2C3_MRIS_R & I2C_MRIS_RIS) == 0)
        ;
    // read data from register
    I2C3_MSA_R = (add << 1) | 1; // add:r/~w=1
    I2C3_MICR_R = I2C_MICR_IC;
    I2C3_MCS_R = I2C_MCS_START | I2C_MCS_RUN | I2C_MCS_STOP;
    while ((I2C3_MRIS_R & I2C_MRIS_RIS) == 0)
        ;
    return I2C3_MDR_R;
}
