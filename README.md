# STM32 数字步进衰减器控制项目

## 描述

本项目旨在使用 STM32 微控制器通过 SPI 接口控制数字步进衰减器（如 HMC624A、PE4302 或其他兼容型号）。代码包含一个硬件抽象层 (HAL)，方便移植到不同的微控制器平台（当前支持 STM32 HAL 和 MSPM0 DriverLib）。

项目包含一个测试程序，该程序会循环设置衰减值从最小值 (0dB) 到最大值 (31.5dB)，然后再返回最小值，步进为 0.5dB。板载 LED 会闪烁以指示程序正在运行。

## 硬件要求

*   **微控制器**: STM32F103C8T6 ("Blue Pill") 或其他 STM32 型号。
*   **衰减器模块**: HMC624A、PE4302 或其他具有 SPI 串行控制接口的数字步进衰减器模块。
*   **编程器/调试器**: ST-Link V2 或兼容的调试器。
*   **连接线**: 杜邦线若干。

## 软件要求

*   **IDE**: STM32CubeIDE (推荐), Keil MDK, IAR Embedded Workbench, 或 PlatformIO。
*   **配置工具**: STM32CubeMX (已集成在 STM32CubeIDE 中)。
*   **编程软件**: STM32 ST-LINK Utility 或 IDE 内置的烧录功能。
*   **驱动**: ST-Link 驱动程序。

## 硬件连接

请根据您的硬件和 `Attenuator.h` 中的配置，将 STM32 与衰减器模块连接起来。以下是一个示例连接（假设使用 SPI1 和 `main.c` 中的默认引脚 PA0/PA1）：

| STM32 引脚        | 衰减器引脚 | 功能                 |
| :---------------- | :--------- | :------------------- |
| PA7 (SPI1\_MOSI)  | SDI        | 串行数据输入         |
| PA5 (SPI1\_SCK)   | SCK        | 串行时钟             |
| PA0               | P/S        | 并行/串行模式选择    |
| PA1               | LE         | 锁存使能             |
| GND               | GND        | 地                   |
| 3.3V 或 5V        | VCC        | 电源 (请检查模块电压) |
| PC13 (板载 LED)   | -          | 程序运行指示         |

**注意**:
*   衰减器的 P/S (Parallel/Serial Mode) 引脚必须连接到 STM32 的 GPIO 并由 `Attenuator_Init` 函数控制。对于串行模式，此引脚通常需要设置为高电平。
*   衰减器的 LE (Latch Enable) 引脚必须连接到 STM32 的 GPIO 并由驱动程序控制。
*   请务必检查衰减器模块所需的工作电压 (VCC)，并连接到 STM32 上相应的电源引脚。

## STM32CubeMX 配置 (参考)

1.  **启用 SPI**:
    *   在 Pinout & Configuration -> Connectivity 中选择 `SPI1` (或您使用的 SPI 外设)。
    *   设置模式为 `Master`。
    *   硬件 NSS 信号设置为 `Disable`。
    *   **参数设置**:
        *   Frame Format: `Motorola`
        *   Data Size: `8 Bits`
        *   First Bit: `MSB First`
        *   Clock Polarity (CPOL): `Low` (通常为 Low，请查阅衰减器手册)
        *   Clock Phase (CPHA): `1 Edge` (通常为 1 Edge，请查阅衰减器手册)
        *   根据需要调整波特率 (Prescaler)。
2.  **配置 GPIO**:
    *   将连接到衰减器 `LE` 的引脚 (例如 `PA1`) 配置为 `GPIO_Output`。
    *   将连接到衰减器 `P/S` 的引脚 (例如 `PA0`) 配置为 `GPIO_Output`。
    *   将连接到板载 LED 的引脚 (例如 `PC13`) 配置为 `GPIO_Output`。
    *   在 GPIO 设置中，可以为这些引脚设置用户标签 (User Label)，如 `LE`, `PS`, `LED`。
3.  **生成代码**: 点击 "Generate Code" (Alt+K)。

## 编译与运行

1.  **选择平台**:
    *   打开 `Core/Inc/Attenuator.h` 文件。
    *   确保文件顶部的平台选择宏已正确设置，例如取消注释 `#define USE_STM32_HAL`。
2.  **编译**: 在 IDE 中构建项目 (例如，在 STM32CubeIDE 中点击锤子图标)。
3.  **连接硬件**: 将 ST-Link 连接到电脑和 STM32 板的 SWD 接口，并将衰减器模块连接到 STM32。
4.  **烧录**: 将编译生成的 `.elf` 或 `.hex` 文件烧录到 STM32 (例如，在 STM32CubeIDE 中点击运行或调试按钮)。
5.  **观察**: 程序运行后，STM32 板上的 LED (PC13) 应开始闪烁。衰减器的衰减值会按照测试程序设定的顺序变化。您需要使用射频信号源和频谱分析仪或功率计来验证衰减效果。

## 代码结构

*   `Core/Inc/Attenuator.h`: 衰减器驱动的头文件。包含平台选择、类型定义、常量定义、句柄结构体以及函数声明。硬件抽象层的定义也在此文件中。
*   `Core/Src/Attenuator.c`: 衰减器驱动的源文件。包含硬件抽象层函数的实现和衰减器控制逻辑的实现。
*   `Core/Src/main.c`: 主应用程序。包含硬件初始化、衰减器驱动初始化以及衰减器测试序列。
*   `Core/Inc/main.h`: 主要的包含文件和引脚定义（由 CubeMX 生成）。
*   `STM32CubeMX (.ioc)`: STM32CubeMX 配置文件。

## 定制与移植

*   **更改 GPIO 引脚**:
    1.  在 STM32CubeMX 中修改引脚分配并重新生成代码。
    2.  修改 `Core/Src/main.c` 中调用 `Attenuator_Init` 函数时传递的 GPIO 端口和引脚参数 (例如 `PS_GPIO_Port`, `PS_Pin`, `LE_GPIO_Port`, `LE_Pin`)，使其与 `main.h` 中 CubeMX 生成的宏匹配。
*   **更改 SPI 外设**:
    1.  在 STM32CubeMX 中配置新的 SPI 外设及其引脚。
    2.  修改 `Core/Src/main.c` 中调用 `Attenuator_Init` 函数时传递的 SPI 句柄 (例如 `&hspi2`)。
*   **支持不同规格的衰减器**:
    *   如果衰减范围或步进不同，修改 `Core/Inc/Attenuator.h` 中的 `ATTENUATOR_MIN_DB`, `ATTENUATOR_MAX_DB`, `ATTENUATOR_STEP_DB` 常量。
    *   如果控制位数或数据格式不同，可能需要修改 `Core/Src/Attenuator.c` 中 `Attenuator_SetAttenuation_SPI` 函数内计算 `data` 的逻辑以及 `Attenuator_Platform_SPI_Transmit` 的调用方式（例如发送多个字节）。
*   **移植到其他平台**:
    1.  在 `Core/Inc/Attenuator.h` 文件顶部的平台选择区域添加新的平台宏，例如 `#define USE_MY_PLATFORM`。
    2.  在 `Core/Inc/Attenuator.h` 的 "平台相关定义和包含文件" 部分添加对应的 `#elif defined(USE_MY_PLATFORM)` 分支，包含该平台的头文件并定义相应的类型 (`Attenuator_GPIO_TypeDef`, `Attenuator_SPI_TypeDef`, `Attenuator_PinState`, `Attenuator_Status` 等)。
    3.  修改 `Attenuator_HandleTypeDef` 结构体以适应新平台的 GPIO/SPI 参数传递方式。
    4.  修改 `Attenuator_Init` 函数声明以适应新平台的参数。
    5.  在 `Core/Src/Attenuator.c` 的硬件抽象层函数 (`Attenuator_Platform_...`) 中添加对应的 `#elif defined(USE_MY_PLATFORM)` 分支，并使用新平台的 API 实现这些函数 (GPIO 写、SPI 传输、延时、NOP)。
    6.  在 `Core/Src/Attenuator.c` 的 `Attenuator_Init` 和 `Attenuator_SetAttenuation_SPI` 函数实现中，添加处理新平台参数和调用的 `#elif` 分支。

## 许可证

请参考项目中的 `LICENSE` 文件。如果未指定，代码可视为在 MIT 许可下发布（请根据实际情况修改）。
