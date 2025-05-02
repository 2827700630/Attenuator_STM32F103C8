#include "Attenuator.h"

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
    if (state == ATTENUATOR_PIN_SET) {
        DL_GPIO_setPins(port, pin);
    } else {
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
    for (i = 0; i < size; i++) {
        while (DL_SPI_isTXFIFOFull(hspi)); // 等待 TX FIFO 非满
        DL_SPI_transmitData8(hspi, data[i]); // 写入数据
        while (!DL_SPI_isRXFIFOEmpty(hspi)); // 等待 RX FIFO 非空
        (void)DL_SPI_receiveData8(hspi); // 读取并丢弃接收到的数据
    }
    while (DL_SPI_isBusy(hspi)); // 等待 SPI 总线空闲
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
 * @brief 平台抽象：微秒级延时函数
 * @param us: 延时微秒数
 */
void Attenuator_Platform_Delay_us(uint32_t us)
{
#if defined(USE_STM32_HAL)
    /* STM32 HAL 微秒延时 - 使用 DWT */
    // 确保 DWT 已启用: CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < cycles);
#elif defined(USE_MSPM0)
    /* MSPM0 平台 - 使用 DriverLib 提供的延时 */
    #ifndef CPUCLK_FREQ
    #warning "CPUCLK_FREQ 未定义，微秒延时可能不准确"
    #define CPUCLK_FREQ (32000000) // 假设一个默认值，例如 32MHz
    #endif
    uint32_t cycles = us * (CPUCLK_FREQ / 1000000);
    if (cycles > 0) {
        DL_Common_delayCycles(cycles);
    } else if (us > 0) {
        Attenuator_Platform_NOP(); // 至少执行一个 NOP
    }
#else
    /* 默认实现为空或 NOP 循环 (非常不精确) */
    volatile uint32_t i, j;
    for (i = 0; i < us; i++) {
        // 这个循环次数需要根据实际 CPU 频率调整
        for (j = 0; j < 10; j++) {
             __NOP();
        }
    }
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