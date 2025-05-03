#include "Attenuator.h"
#include <math.h> // 添加数学函数库，用于log10和pow

/* ===== 1. 硬件抽象层函数实现 ===== */

/**
 * @brief 平台抽象：写入 GPIO 引脚电平
 * @param port: 指向 GPIO 端口/实例的指针 (平台相关类型)
 * @param pin: GPIO 引脚号/掩码 (平台相关类型)
 * @param state: 要设置的引脚状态 (Attenuator_PinState)
 */
void Attenuator_Platform_GPIO_WritePin(Attenuator_GPIO_TypeDef *port, uint32_t pin, Attenuator_PinState state)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    HAL_GPIO_WritePin(port, (uint16_t)pin, state); // STM32 pin 是 uint16_t
#elif defined(USE_MSPM0)
    /* MSPM0 平台 (使用 DriverLib) */
    if (state == ATTENUATOR_PIN_SET)
    {
        DL_GPIO_setPins(port, pin);
    }
    else
    {
        DL_GPIO_clearPins(port, pin);
    }
#else
    /* 默认使用 STM32 HAL */
    HAL_GPIO_WritePin(port, (uint16_t)pin, state);
#endif
}

/**
 * @brief 平台抽象：SPI 传输函数
 * @param hspi: 指向 SPI 句柄/寄存器的指针 (平台相关类型)
 * @param data: 指向要发送数据的指针
 * @param size: 要发送的数据大小 (字节数)
 * @param timeout: 超时时间 (ms) - 可能不使用
 * @return: 传输状态 (Attenuator_Status)
 */
Attenuator_Status Attenuator_Platform_SPI_Transmit(Attenuator_SPI_TypeDef *hspi, uint8_t *data, uint16_t size, uint32_t timeout)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    return HAL_SPI_Transmit(hspi, data, size, timeout);
#elif defined(USE_MSPM0)
    /* MSPM0 平台 (使用 DriverLib) - 假设使用轮询方式 */
    uint16_t i;
    for (i = 0; i < size; i++)
    {
        while (DL_SPI_isTXFIFOFull(hspi))
            ;                                // 等待 TX FIFO 非满
        DL_SPI_transmitData8(hspi, data[i]); // 写入数据
        while (!DL_SPI_isRXFIFOEmpty(hspi))
            ;                            // 等待 RX FIFO 非空
        (void)DL_SPI_receiveData8(hspi); // 读取并丢弃接收到的数据
    }
    while (DL_SPI_isBusy(hspi))
        ; // 等待 SPI 总线空闲
    return ATTENUATOR_OK;
#else
    /* 默认使用 STM32 HAL */
    return HAL_SPI_Transmit(hspi, data, size, timeout);
#endif
}

/**
 * @brief 平台抽象：毫秒级延时函数
 * @param ms: 延时毫秒数
 */
void Attenuator_Platform_Delay_ms(uint32_t ms)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    HAL_Delay(ms);
#elif defined(USE_MSPM0)
/* MSPM0 平台 - 使用 DriverLib 提供的延时 */
#ifndef CPUCLK_FREQ
#warning "CPUCLK_FREQ 未定义，毫秒延时可能不准确"
#define CPUCLK_FREQ (32000000) // 假设一个默认值，例如 32MHz
#endif
    DL_Common_delayCycles(ms * (CPUCLK_FREQ / 1000));
#else
    /* 默认使用 STM32 HAL */
    HAL_Delay(ms);
#endif
}

/**
 * @brief 平台抽象：一个空操作的 NOP 函数
 */
void Attenuator_Platform_NOP(void)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 (ARM Cortex-M) */
    __NOP();
#elif defined(USE_MSPM0)
    /* MSPM0 平台 (ARM Cortex-M0+) */
    __NOP();
#elif defined(__CORTEX_M)
    /* 其他 ARM Cortex-M 平台 */
    __NOP();
#else
    /* 其他平台 - 空操作 */
#endif
}

/* ===== 2. 衰减器功能函数实现 ===== */

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
                                  GPIO_Regs *latch_enable_gpio, uint32_t latch_enable_pinIndex)
{
    Attenuator->hspi = hspi;
    Attenuator->parallel_serial_gpio = parallel_serial_gpio;
    Attenuator->parallel_serial_pinIndex = parallel_serial_pinIndex;
    Attenuator->latch_enable_gpio = latch_enable_gpio;
    Attenuator->latch_enable_pinIndex = latch_enable_pinIndex;

    // 初始化 GPIO 状态
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_RESET);
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_gpio, Attenuator->parallel_serial_pinIndex, ATTENUATOR_PIN_SET);

#elif defined(USE_STM32_HAL) // 添加 STM32 HAL 分支
                                  Attenuator_GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  Attenuator_GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin)
{
    Attenuator->hspi = hspi;
    Attenuator->parallel_serial_port = parallel_serial_port;
    Attenuator->parallel_serial_pin = parallel_serial_pin;
    Attenuator->latch_enable_port = latch_enable_port;
    Attenuator->latch_enable_pin = latch_enable_pin;

    // 初始化 GPIO 状态
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, ATTENUATOR_PIN_SET);

#else // 默认或其他平台 (使用与 STM32 相同的参数)
                                  Attenuator_GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  Attenuator_GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin)
{
    Attenuator->hspi = hspi;
    Attenuator->parallel_serial_port = parallel_serial_port;
    Attenuator->parallel_serial_pin = parallel_serial_pin;
    Attenuator->latch_enable_port = latch_enable_port;
    Attenuator->latch_enable_pin = latch_enable_pin;

    // 初始化 GPIO 状态
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, ATTENUATOR_PIN_SET);
#endif
    return ATTENUATOR_OK;
}

/**
 * @brief 串行模式设置衰减值
 * @param Attenuator: 指向衰减器设备句柄的指针
 * @param attenuation: 目标衰减值（范围由 ATTENUATOR_MIN_DB 和 ATTENUATOR_MAX_DB 定义）
 * @return: 操作状态 (Attenuator_Status)
 * @note 适用于支持串行模式的衰减器，步进由 ATTENUATOR_STEP_DB 定义
 */
Attenuator_Status Attenuator_Set_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation)
{
    Attenuator_Status status;

    // 1. 确保 Parallel/Serial Mode = 1（串行模式）
#if defined(USE_MSPM0)
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_gpio, Attenuator->parallel_serial_pinIndex, ATTENUATOR_PIN_SET);
#elif defined(USE_STM32_HAL)
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, ATTENUATOR_PIN_SET);
#else // 默认或其他平台
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, ATTENUATOR_PIN_SET);
#endif

    // 2. 计算 6 位控制字
    if (attenuation < ATTENUATOR_MIN_DB)
        attenuation = ATTENUATOR_MIN_DB;
    if (attenuation > ATTENUATOR_MAX_DB)
        attenuation = ATTENUATOR_MAX_DB;
    uint8_t data = (uint8_t)(attenuation / ATTENUATOR_STEP_DB);

    // 3. 发送数据（MSB 优先） - 拉低 LE
#if defined(USE_MSPM0)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_RESET);
#elif defined(USE_STM32_HAL)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#else // 默认或其他平台
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#endif

    // 使用平台抽象的 SPI 传输函数
    status = Attenuator_Platform_SPI_Transmit(Attenuator->hspi, &data, 1, ATTENUATOR_SPI_TIMEOUT);
    if (status != ATTENUATOR_OK)
    {
        // 传输失败后，最好将 LE 拉低，避免状态不确定
#if defined(USE_MSPM0)
        Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_RESET);
#elif defined(USE_STM32_HAL)
        Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#else // 默认或其他平台
        Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#endif
        return status; // 发生错误，返回错误码
    }

    // 4. 锁存数据 - 拉高 LE
#if defined(USE_MSPM0)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_SET);
#elif defined(USE_STM32_HAL)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_SET);
#else // 默认或其他平台
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_SET);
#endif

    // 使用 NOP 指令确保至少 30ns 的锁存脉宽
    Attenuator_Platform_NOP();
    Attenuator_Platform_NOP();
    Attenuator_Platform_NOP();

    // 锁存结束 - 拉低 LE
#if defined(USE_MSPM0)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_RESET);
#elif defined(USE_STM32_HAL)
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#else // 默认或其他平台
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
#endif

    return ATTENUATOR_OK;
}

/**
 * @brief 将电压倍数转换为dB值，注释中包含常用电压比对应dB值对照表
 * @param ratio: 电压倍数，如衰减到原来的1/10，则ratio=0.1，如增益到原来的10倍，则ratio=10
 * @return: db值
 * @note: 通用计算函数，不限制返回值范围
 * 常用电压比(增益)对应dB值对照表：
 * 电压比(ratio)| dB值
 * ------------|------
 * 1.0         | 0 dB  (无衰减/增益)
 * 1.1         | 0.83 dB
 * 1.2         | 1.58 dB
 * 1.4         | 2.92 dB
 * 1.5         | 3.52 dB
 * 1.585       | 4 dB (约1.6倍)
 * 2.0         | 6.02 dB (2倍)
 * 2.5         | 7.96 dB
 * 3.16        | 10 dB (约3.2倍)
 * 3.98        | 12 dB (约4倍)
 * 5.0         | 13.98 dB (5倍)
 * 6.3         | 16 dB (约6.3倍)
 * 10.0        | 20 dB (10倍)
 * 20.0        | 26.02 dB (20倍)
 * 100.0       | 40 dB (100倍)
 * 1000.0      | 60 dB (1000倍)
 *
 * 常用电压比(衰减)对应dB值对照表：
 * 电压比(ratio)| dB值
 * ------------|------
 * 0.9         | -0.92 dB
 * 0.8         | -1.94 dB
 * 0.708       | -3.0 dB (约0.71倍, 功率减半点)
 * 0.5         | -6.02 dB (电压减半)
 * 0.316       | -10 dB (约0.32倍)
 * 0.2         | -13.98 dB
 * 0.1         | -20 dB (减少一个数量级)
 * 0.01        | -40 dB (减少两个数量级)
 * 0.001       | -60 dB (减少三个数量级)
 */
float VoltageRatioToDb(float ratio)
{
    // 确保ratio为正值
    if (ratio <= 0.0f)
        return 0.0f; // 如果输入无效，返回0dB

    // 电压比转换为dB: dB = 20 * log10(ratio)
    return 20.0f * log10f(ratio);
}

/**
 * @brief 将dB值转换为电压倍数，注释中包含常用dB值对应电压比对照表
 * @param db: db值，如输入-10dB则返回约0.316(1/3.16)，如输入10dB则返回约3.16(1/0.316)
 * @return: 电压倍数
 * @note: 通用计算函数，不限制输入范围
 * 常用dB值对应电压比对照表：
 * dB值    | 电压比(ratio)| 说明
 * --------|-------------|------------------------
 * -20 dB  | 0.1         | 0.1倍 (减少一个数量级)
 * -10 dB  | 0.316       | 约0.32倍
 * -6 dB   | 0.501       | 约0.5倍 (电压减半)
 * -3 dB   | 0.708       | 约0.71倍 (功率减半点)
 * 0 dB    | 1.0         | 无衰减/增益
 * 1 dB    | 1.122       | 约1.12倍
 * 2 dB    | 1.259       | 约1.26倍
 * 3 dB    | 1.413       | 约1.41倍 (功率减半点)
 * 6 dB    | 1.995       | 约2倍
 * 10 dB   | 3.162       | 约3.16倍 (一个数量级)
 * 12 dB   | 3.981       | 约4倍
 * 20 dB   | 10.0        | 10倍
 * 30 dB   | 31.62       | 约31.6倍
 * 40 dB   | 100.0       | 100倍 (两个数量级)
 * 60 dB   | 1000.0      | 1000倍 (三个数量级)
 */
float DbToVoltageRatio(float db)
{
    // dB转换为电压比: ratio = 10^(dB/20)
    return powf(10.0f, db / 20.0f);
}

/**
 * @brief 使用电压衰减倍数设置衰减值
 * @param Attenuator: 指向衰减器设备句柄的指针
 * @param ratio: 电压倍数，如衰减到原来的1/10，则ratio=0.1
 * @return: 操作状态 (Attenuator_Status)
 */
Attenuator_Status Attenuator_Set_Ratio(Attenuator_HandleTypeDef *Attenuator, float ratio)
{
    float db;

    // 输入检查
    if (ratio <= 0.0f)
    {
        return ATTENUATOR_ERROR; // 无效的衰减比例
    }

    // 转换为dB值
    db = -VoltageRatioToDb(ratio);

    // 限制在衰减器支持的范围内
    if (db < ATTENUATOR_MIN_DB)
        db = ATTENUATOR_MIN_DB;
    if (db > ATTENUATOR_MAX_DB)
        db = ATTENUATOR_MAX_DB;

    // 设置衰减值
    return Attenuator_Set_SPI(Attenuator, db);
}