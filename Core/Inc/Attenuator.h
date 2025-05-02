#ifndef INC_ATTENUATOR_H_
#define INC_ATTENUATOR_H_

/**
 * 
 * 衰减器数据手册摘要
 * 
 * - HMC624A: 0.1-6.0 GHz, 6位数字控制, 0.5 dB/步进, 最大31.5 dB
 * - PE4302: DC-4.0 GHz, 6位数字控制, 0.5 dB/步进, 最大31.5 dB
 * (详细数据请参考各自数据手册)
 */

/* 在下面选择你使用的平台 */
#define USE_STM32_HAL // 使用STM32 HAL库
// #define USE_MSPM0 // 使用MSPM0

/* ===== 1. 平台相关定义和包含文件 ===== */
#if defined(USE_STM32_HAL)
/* STM32 HAL 平台 */
#include "main.h" // 包含 STM32 HAL 定义
#include "spi.h"  // 包含 SPI 头文件

/* STM32 类型定义 */
#define Attenuator_GPIO_TypeDef GPIO_TypeDef
#define Attenuator_SPI_TypeDef SPI_HandleTypeDef
#define Attenuator_PinState GPIO_PinState
#define ATTENUATOR_PIN_RESET GPIO_PIN_RESET
#define ATTENUATOR_PIN_SET GPIO_PIN_SET
#define Attenuator_Status HAL_StatusTypeDef
#define ATTENUATOR_OK HAL_OK
#define ATTENUATOR_ERROR HAL_ERROR
#define ATTENUATOR_BUSY HAL_BUSY
#define ATTENUATOR_TIMEOUT HAL_TIMEOUT

#elif defined(USE_MSPM0)
/* MSPM0 平台 (使用 DriverLib) */
#include "ti_msp_dl_config.h" // 包含 MSPM0 SysConfig 生成的配置头文件
#include <stdint.h>

/* MSPM0 类型定义 */
#define Attenuator_GPIO_TypeDef GPIO_Regs // GPIO 寄存器结构体指针
#define Attenuator_SPI_TypeDef SPI_Regs * // SPI 寄存器结构体指针
#define Attenuator_PinState uint8_t
#define ATTENUATOR_PIN_RESET 0
#define ATTENUATOR_PIN_SET 1
#define Attenuator_Status int // 使用简单的 int 作为状态码
#define ATTENUATOR_OK 0
#define ATTENUATOR_ERROR 1
#define ATTENUATOR_BUSY 2
#define ATTENUATOR_TIMEOUT 3

#else /* 默认使用 STM32 HAL */
#warning "未指定平台，默认使用 STM32 HAL"
#include "main.h" // 包含 STM32 HAL 定义
#include "spi.h"  // 包含 SPI 头文件

/* 默认使用 STM32 类型定义 */
#define Attenuator_GPIO_TypeDef GPIO_TypeDef
#define Attenuator_SPI_TypeDef SPI_HandleTypeDef
#define Attenuator_PinState GPIO_PinState
#define ATTENUATOR_PIN_RESET GPIO_PIN_RESET
#define ATTENUATOR_PIN_SET GPIO_PIN_SET
#define Attenuator_Status HAL_StatusTypeDef
#define ATTENUATOR_OK HAL_OK
#define ATTENUATOR_ERROR HAL_ERROR
#define ATTENUATOR_BUSY HAL_BUSY
#define ATTENUATOR_TIMEOUT HAL_TIMEOUT
#endif

/* ===== 2. 衰减器常量定义 ===== */
#define ATTENUATOR_MIN_DB 0.0f     // 最小衰减值 (dB)
#define ATTENUATOR_MAX_DB 31.5f    // 最大衰减值 (dB)
#define ATTENUATOR_STEP_DB 0.5f    // 衰减步进 (dB)
#define ATTENUATOR_SPI_TIMEOUT 100 // SPI 超时时间 (ms) - 在轮询模式下可能不直接使用

/* ===== 3. 衰减器设备句柄结构体定义 ===== */
typedef struct
{
    Attenuator_SPI_TypeDef *hspi; // 指向 SPI 句柄/寄存器的指针
#if defined(USE_MSPM0)
    // MSPM0 DriverLib 通常使用实例名称和引脚掩码
    IOMUX_PINCM_Regs *parallel_serial_iomux; // Parallel/Serial Mode 引脚的 IOMUX 配置指针
    uint32_t parallel_serial_pinIndex;       // Parallel/Serial Mode 引脚索引 (例如 DL_GPIO_PIN_0)
    GPIO_Regs *parallel_serial_gpio;         // Parallel/Serial Mode 引脚的 GPIO 实例指针
    IOMUX_PINCM_Regs *latch_enable_iomux;    // Latch Enable Input 引脚的 IOMUX 配置指针
    uint32_t latch_enable_pinIndex;          // Latch Enable Input 引脚索引
    GPIO_Regs *latch_enable_gpio;            // Latch Enable Input 引脚的 GPIO 实例指针
#elif defined(USE_STM32_HAL)
    // STM32 平台使用 port/pin
    Attenuator_GPIO_TypeDef *parallel_serial_port; // Parallel/Serial Mode 引脚的 GPIO 端口
    uint16_t parallel_serial_pin;                  // Parallel/Serial Mode 引脚号
    Attenuator_GPIO_TypeDef *latch_enable_port;    // Latch Enable Input 引脚的 GPIO 端口
    uint16_t latch_enable_pin;                     // Latch Enable Input 引脚号
#else // 默认或其他平台 (使用与 STM32 相同的结构)
    Attenuator_GPIO_TypeDef *parallel_serial_port; // Parallel/Serial Mode 引脚的 GPIO 端口
    uint16_t parallel_serial_pin;                  // Parallel/Serial Mode 引脚号
    Attenuator_GPIO_TypeDef *latch_enable_port;    // Latch Enable Input 引脚的 GPIO 端口
    uint16_t latch_enable_pin;                     // Latch Enable Input 引脚号
#endif
} Attenuator_HandleTypeDef;

/* ===== 4. 硬件抽象层函数声明 ===== */
/**
 * @brief 平台抽象：写入 GPIO 引脚电平
 * @param port: 指向 GPIO 端口/实例的指针 (平台相关类型)
 * @param pin: GPIO 引脚号/掩码 (平台相关类型)
 * @param state: 要设置的引脚状态 (Attenuator_PinState)
 */
void Attenuator_Platform_GPIO_WritePin(Attenuator_GPIO_TypeDef *port, uint32_t pin, Attenuator_PinState state);

/**
 * @brief 平台抽象：SPI 传输函数
 * @param hspi: 指向 SPI 句柄/寄存器的指针 (平台相关类型)
 * @param data: 指向要发送数据的指针
 * @param size: 要发送的数据大小 (字节数)
 * @param timeout: 超时时间 (ms) - 可能不使用
 * @return: 传输状态 (Attenuator_Status)
 */
Attenuator_Status Attenuator_Platform_SPI_Transmit(Attenuator_SPI_TypeDef *hspi, uint8_t *data, uint16_t size, uint32_t timeout);

/**
 * @brief 平台抽象：毫秒级延时函数
 * @param ms: 延时毫秒数
 */
void Attenuator_Platform_Delay_ms(uint32_t ms);

/**
 * @brief 平台抽象：微秒级延时函数
 * @param us: 延时微秒数
 */
void Attenuator_Platform_Delay_us(uint32_t us);

/**
 * @brief 平台抽象：一个空操作的 NOP 函数
 */
void Attenuator_Platform_NOP(void);

/* ===== 5. 衰减器功能函数声明 ===== */
/**
 * @brief 初始化衰减器设备句柄
 * @param Attenuator: 指向衰减器设备句柄的指针
 * @param hspi: 指向要使用的 SPI 句柄/寄存器的指针
 * @param parallel_serial_port: Parallel/Serial Mode 引脚的 GPIO 端口/实例
 * @param parallel_serial_pin: Parallel/Serial Mode 引脚号/掩码
 * @param latch_enable_port: Latch Enable Input 引脚的 GPIO 端口/实例
 * @param latch_enable_pin: Latch Enable Input 引脚号/掩码
 * @return: 操作状态 (Attenuator_Status)
 * @note 对于 MSPM0，port/pin 参数应对应 SysConfig 生成的 GPIO 实例和引脚掩码/索引
 */
Attenuator_Status Attenuator_Init(Attenuator_HandleTypeDef *Attenuator, Attenuator_SPI_TypeDef *hspi,
#if defined(USE_MSPM0)
                                  GPIO_Regs *parallel_serial_gpio, uint32_t parallel_serial_pinIndex,
                                  GPIO_Regs *latch_enable_gpio, uint32_t latch_enable_pinIndex);
#elif defined(USE_STM32_HAL)
                                  Attenuator_GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  Attenuator_GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin);
#else // 默认或其他平台 (使用与 STM32 相同的参数类型)
                                  Attenuator_GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  Attenuator_GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin);
#endif

/**
 * @brief 串行模式设置衰减值
 * @param Attenuator: 指向衰减器设备句柄的指针
 * @param attenuation: 目标衰减值（范围由 ATTENUATOR_MIN_DB 和 ATTENUATOR_MAX_DB 定义）
 * @return: 操作状态 (Attenuator_Status)
 * @note 适用于支持串行模式的衰减器，步进由 ATTENUATOR_STEP_DB 定义
 */
Attenuator_Status Attenuator_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation);

#endif /* INC_ATTENUATOR_H_ */
