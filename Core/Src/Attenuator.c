/**
 * @file    Attenuator.c
 * @brief   数字衰减器控制驱动源文件
 */

#include "Attenuator.h"

/**
 * @brief  初始化衰减器设备
 * @param  Attenuator: 指向衰减器设备句柄的指针
 * @param  hspi: 所使用的 SPI 句柄
 * @param  parallel_serial_port: P/S 控制引脚对应的 GPIO 端口
 * @param  parallel_serial_pin: P/S 控制引脚编号
 * @param  latch_enable_port: LE 控制引脚对应的 GPIO 端口
 * @param  latch_enable_pin: LE 控制引脚编号
 * @retval HAL_StatusTypeDef: 初始化状态 (HAL_OK / HAL_ERROR)
 */
HAL_StatusTypeDef Attenuator_Init(Attenuator_HandleTypeDef *Attenuator, SPI_HandleTypeDef *hspi,
                                  GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin)
{
    /* 初始化句柄参数 */
    Attenuator->hspi = hspi;
    Attenuator->parallel_serial_port = parallel_serial_port;
    Attenuator->parallel_serial_pin = parallel_serial_pin;
    Attenuator->latch_enable_port = latch_enable_port;
    Attenuator->latch_enable_pin = latch_enable_pin;

    /* 默认拉低 LE 锁存引脚，拉高 P/S 引脚以选择串行模式 */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, GPIO_PIN_SET);

    return HAL_OK;
}

/**
 * @brief  通过 SPI 接口设置衰减器的衰减值
 * @note   该函数内部会自动控制 P/S 及 LE 引脚完成锁存
 * @param  Attenuator: 指向衰减器设备句柄的指针
 * @param  attenuation: 目标衰减值 (dB)，范围 0.0 ~ 31.5
 * @retval HAL_StatusTypeDef: 设置状态 (HAL_OK / HAL_ERROR / HAL_TIMEOUT)
 */
HAL_StatusTypeDef Attenuator_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation)
{
    HAL_StatusTypeDef status;

    /* 1. 确保 P/S 引脚为高电平，进入串行控制模式 */
    HAL_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, GPIO_PIN_SET);

    /* 2. 边界限幅并计算 6 位控制字 (如每步 0.5dB的HMC624A) */
    if (attenuation < ATTENUATOR_MIN_DB)
        attenuation = ATTENUATOR_MIN_DB;
    if (attenuation > ATTENUATOR_MAX_DB)
        attenuation = ATTENUATOR_MAX_DB;
    uint8_t data = (uint8_t)(attenuation / ATTENUATOR_STEP_DB);

    /* 3. 在 SPI 发送数据时保持 LE 为低电平 */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(Attenuator->hspi, &data, 1, ATTENUATOR_SPI_TIMEOUT);
    if (status != HAL_OK)
    {
        /* 若传输错误，确保 LE 保持低电平，避免锁存错误数据 */
        HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);
        return status;
    }

    /* 4. SPI 传输完成后，拉高 LE 产生锁存脉冲，锁存数据至内部寄存器 */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_SET);

    // /* 至少保持约 30ns 的数据锁存稳定时间，此处简单的使用系统 NOP 指令提供延时 */
    // __NOP();
    // __NOP();
    // __NOP();
    HAL_Delay(1); // 1ms 延时

    /* 5. 锁存结束，拉低 LE */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);

    return HAL_OK;
}

/**
 * @brief  通过 SPI 接口设置 PE43711 衰减器的衰减值
 * @note   该函数内部会自动控制 P/S 及 LE 引脚完成锁存
 *         PE43711 是 7 位数字衰减器，通过 8-bit SPI 写入，数据格式一般为 MSB first。
 *         实际有效数据占 7 bits（0.25dB 步进，最大 31.75dB）。
 * @param  Attenuator: 指向衰减器设备句柄的指针
 * @param  attenuation: 目标衰减值 (dB)，范围 0.0 ~ 31.75
 * @retval HAL_StatusTypeDef: 设置状态 (HAL_OK / HAL_ERROR / HAL_TIMEOUT)
 */
HAL_StatusTypeDef Attenuator_PE43711_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation)
{
    HAL_StatusTypeDef status;

    /* 1. 确保 P/S 引脚为高电平，进入串行控制模式 */
    HAL_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, GPIO_PIN_SET);

    /* 2. 边界限幅并计算控制字 (每步 0.25dB的PE43711) */
    if (attenuation < ATTENUATOR_MIN_DB)
        attenuation = ATTENUATOR_MIN_DB;
    if (attenuation > PE43711_MAX_DB)
        attenuation = PE43711_MAX_DB;
    uint8_t val = (uint8_t)(attenuation / PE43711_STEP_DB);

    /* 
     * 根据时序图，PE43711 需要先接收 D0 (0.25dB位)，最后接收 D7 (必须为逻辑低电平)。 
     * 而 STM32 的 SPI 通常默认配置为 MSB First (最高位先发)。
     * 因此我们需要将 val 进行 8位比特翻转 (Bit-Reverse)，使得原本的 LSB 被移到 MSB 并最先发出。
     */
    uint8_t data = 0;
    for (int i = 0; i < 8; i++)
    {
        if (val & (1 << i))
        {
            data |= (1 << (7 - i));
        }
    }

    /* 3. 在 SPI 发送数据时保持 LE 为低电平 */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(Attenuator->hspi, &data, 1, ATTENUATOR_SPI_TIMEOUT);
    if (status != HAL_OK)
    {
        /* 若传输错误，确保 LE 保持低电平，避免锁存错误数据 */
        HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);
        return status;
    }

    /* 4. SPI 传输完成后，拉高 LE 产生锁存脉冲，锁存数据至内部寄存器 */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_SET);

    HAL_Delay(1); // 1ms 延时，确保锁存完成

    /* 5. 锁存结束，拉低 LE */
    HAL_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, GPIO_PIN_RESET);

    return HAL_OK;
}
