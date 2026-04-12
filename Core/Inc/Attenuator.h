/**
 * @file    Attenuator.h
 * @brief   数字衰减器控制驱动头文件 (适用 HMC624A, PE4302 等)
 * 
 * 衰减器数据手册摘要:
 * - HMC624A: 0.1-6.0 GHz, 6位数字控制, 0.5 dB/步进, 最大31.5 dB
 * - PE4302: DC-4.0 GHz, 6位数字控制, 0.5 dB/步进, 最大31.5 dB
 * - PE43711: 0.1-6.0 GHz, 7位数字控制, 0.25 dB/步进, 最大31.75 dB
 * (详细数据请参考各自数据手册)
 */

#ifndef INC_ATTENUATOR_H_
#define INC_ATTENUATOR_H_

#include "main.h"
#include "spi.h"

/**
 * @defgroup Attenuator_Constants 衰减器常量定义
 * @{
 */
#define ATTENUATOR_MIN_DB 0.0f        /**< 最小衰减值 (dB) */
#define ATTENUATOR_MAX_DB 31.5f       /**< 最大衰减值 (dB) (HMC624A/PE4302) */
#define ATTENUATOR_STEP_DB 0.5f       /**< 衰减步进 (dB) (HMC624A/PE4302) */
#define PE43711_MAX_DB 31.75f         /**< 最大衰减值 (dB) (PE43711) */
#define PE43711_STEP_DB 0.25f         /**< 衰减步进 (dB) (PE43711) */
#define ATTENUATOR_SPI_TIMEOUT 100    /**< SPI 超时时间 (ms) */
/**
 * @}
 */

/**
 * @brief 衰减器设备句柄结构体
 */
typedef struct
{
    SPI_HandleTypeDef *hspi;               /**< 指向 SPI 句柄的指针 */
    GPIO_TypeDef *parallel_serial_port;    /**< Parallel/Serial 模式控制引脚控制端口 */
    uint16_t parallel_serial_pin;          /**< Parallel/Serial 模式控制引脚编号 */
    GPIO_TypeDef *latch_enable_port;       /**< Latch Enable (LE) 锁存使能引脚端口 */
    uint16_t latch_enable_pin;             /**< Latch Enable (LE) 锁存使能引脚编号 */
} Attenuator_HandleTypeDef;

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
                                  GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin);

/**
 * @brief  通过 SPI 接口设置衰减器的衰减值
 * @note   该函数内部会自动控制 P/S 及 LE 引脚完成锁存
 * @param  Attenuator: 指向衰减器设备句柄的指针
 * @param  attenuation: 目标衰减值 (dB)，范围 0.0 ~ 31.5
 * @retval HAL_StatusTypeDef: 设置状态 (HAL_OK / HAL_ERROR / HAL_TIMEOUT)
 */
HAL_StatusTypeDef Attenuator_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation);

/**
 * @brief  通过 SPI 接口设置 PE43711 衰减器的衰减值
 * @note   PE43711 是 7 位数字衰减器，步进为 0.25dB，最大 31.75dB
 * @param  Attenuator: 指向衰减器设备句柄的指针
 * @param  attenuation: 目标衰减值 (dB)，范围 0.0 ~ 31.75
 * @retval HAL_StatusTypeDef: 设置状态 (HAL_OK / HAL_ERROR / HAL_TIMEOUT)
 */
HAL_StatusTypeDef Attenuator_PE43711_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation);

#endif /* INC_ATTENUATOR_H_ */
