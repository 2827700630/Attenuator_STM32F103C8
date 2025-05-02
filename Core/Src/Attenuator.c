#include "Attenuator.h"

/* ===== 1. 硬件抽象层函数实现 ===== */

/**
 * @brief 平台抽象：写入 GPIO 引脚电平
 */
void Attenuator_Platform_GPIO_WritePin(Attenuator_GPIO_TypeDef *port, uint32_t pin, Attenuator_PinState state)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    HAL_GPIO_WritePin(port, (uint16_t)pin, state); // STM32 pin 是 uint16_t
#elif defined(USE_MSPM0)
    /* MSPM0 平台 (使用 DriverLib) */
    // port 是 GPIO_Regs* (例如 GPIOA), pin 是引脚掩码 (例如 DL_GPIO_PIN_0)
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
 */
Attenuator_Status Attenuator_Platform_SPI_Transmit(Attenuator_SPI_TypeDef *hspi, uint8_t *data, uint16_t size, uint32_t timeout)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    return HAL_SPI_Transmit(hspi, data, size, timeout);
#elif defined(USE_MSPM0)
    /* MSPM0 平台 (使用 DriverLib) - 假设使用轮询方式 */
    // hspi 是 SPI_Regs* (例如 SPI0)
    uint16_t i;
    for (i = 0; i < size; i++)
    {
        // 等待 TX FIFO 非满
        while (DL_SPI_isTXFIFOFull(hspi))
        {
            // 可选：添加超时处理
        }
        // 写入数据
        DL_SPI_transmitData8(hspi, data[i]);
        // 等待 RX FIFO 非空 (确保传输完成)
        while (!DL_SPI_isRXFIFOEmpty(hspi))
        {
            // 可选：添加超时处理
        }
        // 读取并丢弃接收到的数据，以清空 RX FIFO
        (void)DL_SPI_receiveData8(hspi);
    }
    // 等待 SPI 总线空闲
    while (DL_SPI_isBusy(hspi))
    {
        // 可选：添加超时处理
    }
    return ATTENUATOR_OK;
#else
    /* 默认使用 STM32 HAL */
    return HAL_SPI_Transmit(hspi, data, size, timeout);
#endif
}

/**
 * @brief 平台抽象：毫秒级延时函数
 */
void Attenuator_Platform_Delay_ms(uint32_t ms)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 平台 */
    HAL_Delay(ms);
#elif defined(USE_MSPM0)
/* MSPM0 平台 - 使用 DriverLib 提供的延时 */
// 假设 CPU 时钟频率已定义为 CPUCLK_FREQ (在 ti_msp_dl_config.h 或其他地方)
// 注意：DL_Common_delayCycles 的精度依赖于 CPUCLK_FREQ 的准确性
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
 * @brief 平台抽象：微秒级延时函数
 */
void Attenuator_Platform_Delay_us(uint32_t us)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 微秒延时 - 使用 DWT */
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < cycles)
        ;
#elif defined(USE_MSPM0)
/* MSPM0 平台 - 使用 DriverLib 提供的延时 */
#ifndef CPUCLK_FREQ
#warning "CPUCLK_FREQ 未定义，微秒延时可能不准确"
#define CPUCLK_FREQ (32000000) // 假设一个默认值，例如 32MHz
#endif
    // 对于非常短的延时，直接计算周期数可能更准确
    // 注意：如果 us * (CPUCLK_FREQ / 1000000) 结果小于 1，延时会无效
    uint32_t cycles = us * (CPUCLK_FREQ / 1000000);
    if (cycles > 0)
    {
        DL_Common_delayCycles(cycles);
    }
    else if (us > 0)
    {
        // 对于小于一个周期的延时，至少执行一个 NOP
        Attenuator_Platform_NOP();
    }
#else
    /* 默认实现为空或 NOP 循环 */
    volatile uint32_t i;
    for (i = 0; i < us; i++)
    {
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
        __NOP();
    }
#endif
}

/**
 * @brief 平台抽象：一个空操作的 NOP 函数
 */
void Attenuator_Platform_NOP(void)
{
#if defined(USE_STM32_HAL) || defined(__CORTEX_M) || defined(USE_MSPM0)
    /* ARM Cortex-M 平台 (包括 STM32 和 MSPM0) */
    __NOP();
#else
    /* 其他平台 - 空操作 */
#endif
}

/* ===== 2. 衰减器功能函数实现 ===== */

/**
 * @brief 初始化衰减器设备句柄
 */
Attenuator_Status Attenuator_Init(Attenuator_HandleTypeDef *Attenuator, Attenuator_SPI_TypeDef *hspi,
#if defined(USE_MSPM0)
                                  GPIO_Regs *parallel_serial_gpio, uint32_t parallel_serial_pinIndex,
                                  GPIO_Regs *latch_enable_gpio, uint32_t latch_enable_pinIndex)
{
    Attenuator->hspi = hspi;
    // MSPM0: 存储 GPIO 实例和引脚索引/掩码
    Attenuator->parallel_serial_gpio = parallel_serial_gpio;
    Attenuator->parallel_serial_pinIndex = parallel_serial_pinIndex;
    Attenuator->latch_enable_gpio = latch_enable_gpio;
    Attenuator->latch_enable_pinIndex = latch_enable_pinIndex;

    // 初始化 GPIO 状态 - 使用平台抽象函数
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_gpio, Attenuator->latch_enable_pinIndex, ATTENUATOR_PIN_RESET);
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_gpio, Attenuator->parallel_serial_pinIndex, ATTENUATOR_PIN_SET);

#else // 其他平台
                                  Attenuator_GPIO_TypeDef *parallel_serial_port, uint16_t parallel_serial_pin,
                                  Attenuator_GPIO_TypeDef *latch_enable_port, uint16_t latch_enable_pin)
{
    Attenuator->hspi = hspi;
    Attenuator->parallel_serial_port = parallel_serial_port;
    Attenuator->parallel_serial_pin = parallel_serial_pin;
    Attenuator->latch_enable_port = latch_enable_port;
    Attenuator->latch_enable_pin = latch_enable_pin;

    // 初始化 GPIO 状态 - 使用平台抽象函数
    Attenuator_Platform_GPIO_WritePin(Attenuator->latch_enable_port, Attenuator->latch_enable_pin, ATTENUATOR_PIN_RESET);
    Attenuator_Platform_GPIO_WritePin(Attenuator->parallel_serial_port, Attenuator->parallel_serial_pin, ATTENUATOR_PIN_SET);
#endif
    return ATTENUATOR_OK;
}

/**
 * @brief 串行模式设置衰减值
 */
Attenuator_Status Attenuator_SetAttenuation_SPI(Attenuator_HandleTypeDef *Attenuator, float attenuation)
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