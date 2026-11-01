/*
 * bmp180.c
 *
 *  Created on: Nov 1, 2026
 *      Author: LENOVO
 */


#include "bmp180.h"
#include <math.h>



static HAL_StatusTypeDef rd(BMP180_t *d, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(d->hi2c, BMP180_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static HAL_StatusTypeDef wr(BMP180_t *d, uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(d->hi2c, BMP180_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

HAL_StatusTypeDef BMP180_Init(BMP180_t *d, I2C_HandleTypeDef *hi2c, BMP180_OSS oss)
{
    uint8_t id, c[22];

    d->hi2c = hi2c;
    d->oss  = oss;

    if (rd(d, REG_CHIP_ID, &id, 1) != HAL_OK) return HAL_ERROR;
    if (id != BMP180_CHIP_ID)                 return HAL_ERROR;

    if (rd(d, REG_CALIB_START, c, 22) != HAL_OK) return HAL_ERROR;

    d->AC1 = (int16_t)((c[0]  << 8) | c[1]);
    d->AC2 = (int16_t)((c[2]  << 8) | c[3]);
    d->AC3 = (int16_t)((c[4]  << 8) | c[5]);
    d->AC4 = (uint16_t)((c[6] << 8) | c[7]);
    d->AC5 = (uint16_t)((c[8] << 8) | c[9]);
    d->AC6 = (uint16_t)((c[10] << 8) | c[11]);
    d->B1  = (int16_t)((c[12] << 8) | c[13]);
    d->B2  = (int16_t)((c[14] << 8) | c[15]);
    d->MB  = (int16_t)((c[16] << 8) | c[17]);
    d->MC  = (int16_t)((c[18] << 8) | c[19]);
    d->MD  = (int16_t)((c[20] << 8) | c[21]);

    return HAL_OK;
}

HAL_StatusTypeDef BMP180_ReadTemperature(BMP180_t *d, int32_t *temp_x10)
{
    uint8_t b[2];

    if (wr(d, REG_CTRL_MEAS, CMD_TEMP) != HAL_OK) return HAL_ERROR;
    HAL_Delay(5);                                  /* 4.5 ms max */
    if (rd(d, REG_OUT_MSB, b, 2) != HAL_OK)       return HAL_ERROR;

    int32_t UT = (b[0] << 8) | b[1];

    int32_t X1 = ((UT - (int32_t)d->AC6) * (int32_t)d->AC5) >> 15;
    int32_t X2 = ((int32_t)d->MC << 11) / (X1 + d->MD);
    d->B5 = X1 + X2;

    *temp_x10 = (d->B5 + 8) >> 4;
    return HAL_OK;
}

HAL_StatusTypeDef BMP180_ReadPressure(BMP180_t *d, int32_t *press_pa)
{
    uint8_t b[3];
    static const uint8_t wait_ms[4] = {5, 8, 14, 26};

    /* B5 must be fresh -> read temperature first */
    int32_t t;
    if (BMP180_ReadTemperature(d, &t) != HAL_OK) return HAL_ERROR;

    if (wr(d, REG_CTRL_MEAS, CMD_PRESSURE + (d->oss << 6)) != HAL_OK) return HAL_ERROR;
    HAL_Delay(wait_ms[d->oss]);
    if (rd(d, REG_OUT_MSB, b, 3) != HAL_OK) return HAL_ERROR;

    int32_t UP = (((int32_t)b[0] << 16) | ((int32_t)b[1] << 8) | b[2]) >> (8 - d->oss);

    int32_t B6 = d->B5 - 4000;
    int32_t X1 = ((int32_t)d->B2 * ((B6 * B6) >> 12)) >> 11;
    int32_t X2 = ((int32_t)d->AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = ((((int32_t)d->AC1 * 4 + X3) << d->oss) + 2) >> 2;

    X1 = ((int32_t)d->AC3 * B6) >> 13;
    X2 = ((int32_t)d->B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = ((uint32_t)d->AC4 * (uint32_t)(X3 + 32768)) >> 15;
    uint32_t B7 = ((uint32_t)UP - B3) * (50000 >> d->oss);

    int32_t p = (B7 < 0x80000000) ? (int32_t)((B7 << 1) / B4)
                                  : (int32_t)((B7 / B4) << 1);

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p  = p + ((X1 + X2 + 3791) >> 4);

    *press_pa = p;
    return HAL_OK;
}

float BMP180_Altitude(int32_t press_pa, float sea_level_pa)
{
    return 44330.0f * (1.0f - powf((float)press_pa / sea_level_pa, 0.1903f));
}
