/*
 * bmp180.h
 *
 *  Created on: Nov 1, 2026
 *      Author: LENOVO
 */

#ifndef INC_BMP180_H_
#define INC_BMP180_H_


#include "stm32f1xx_hal.h"
#include <stdint.h>
#define REG_CALIB_START  0xAA
#define REG_CHIP_ID      0xD0
#define REG_CTRL_MEAS    0xF4
#define REG_OUT_MSB      0xF6
#define CMD_TEMP         0x2E
#define CMD_PRESSURE     0x34

#define BMP180_ADDR      (0x77 << 1)   /* HAL needs 8-bit address = 0xEE */
#define BMP180_CHIP_ID   0x55

/* Oversampling: 0=ultra low power ... 3=ultra high resolution */
typedef enum {
    BMP180_OSS_0 = 0,
    BMP180_OSS_1 = 1,
    BMP180_OSS_2 = 2,
    BMP180_OSS_3 = 3
} BMP180_OSS;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    BMP180_OSS oss;
    /* calibration coefficients */
    int16_t  AC1, AC2, AC3;
    uint16_t AC4, AC5, AC6;
    int16_t  B1, B2, MB, MC, MD;
    int32_t  B5;                       /* shared between temp & pressure calc */
} BMP180_t;

HAL_StatusTypeDef BMP180_Init(BMP180_t *dev, I2C_HandleTypeDef *hi2c, BMP180_OSS oss);
HAL_StatusTypeDef BMP180_ReadTemperature(BMP180_t *dev, int32_t *temp_x10);   /* 0.1 degC */
HAL_StatusTypeDef BMP180_ReadPressure(BMP180_t *dev, int32_t *press_pa);      /* Pa */
float             BMP180_Altitude(int32_t press_pa, float sea_level_pa);      /* metres */



#endif /* INC_BMP180_H_ */
