# 蓝牙 OTA 固件升级 — 企业实训文档

> **项目**: PanelPro (STM32F407VET6 + HC-08 BLE 4.0)
> **培训对象**: 嵌入式实习生
> **导师**: Claude
> **开始日期**: 2026-07-17

---

## 目录

1. [培训目标](#1-培训目标)
2. [系统架构概览](#2-系统架构概览)
3. [Flash 分区方案](#3-flash-分区方案)
4. [OTA 通信协议](#4-ota-通信协议)
5. [第 1 课：蓝牙 Echo 测试](#5-第-1-课蓝牙-echo-测试) ✅
6. [第 2 课：Bootloader 骨架](#6-第-2-课bootloader-骨架) 🔥
7. [培训日志](#7-培训日志)

---

## 1. 培训目标

通过 6 个步骤，从零掌握企业级蓝牙 OTA 固件升级的完整实现：

| 步骤 | 内容 | 状态 |
|------|------|------|
| 第 1 课 | 蓝牙 Echo 测试 — 验证 HC-08 ↔ STM32 双向通信 | ✅ 完成 |
| 第 2 课 | Bootloader 骨架 — 独立工程，能跳转到 App | ✅ 完成 |
| 第 3 课 | App 工程重定位 — 让 App 运行在 0x0800C000 | ✅ 完成 |
| 第 4 课 | Flash 操作 + 协议解析 — 接收固件帧，写入 Flash | 🔥 任务 4 进行中 |
| 第 5 课 | App 端 OTA 触发 — LVGL 按钮触发升级 | ⏳ 待开始 |
| 第 6 课 | 异常处理 + 恢复机制 — 超时、CRC 校验、强制恢复 | ⏳ 待开始 |

---

## 2. 系统架构概览

### 2.1 硬件连接

```
┌─────────────┐     UART (9600bps)      ┌──────────────┐
│   HC-08     │ TX ────────────────→ RX │  STM32F407   │
│  BLE 4.0    │ RX ←──────────────── TX │   (PA3/PD5)  │
│  模块       │                          │  USART2      │
└──────┬──────┘                          └──────────────┘
       │ BLE
       │
  ┌────┴────┐
  │  手机   │
  │ BLE App │
  └─────────┘
```

### 2.2 HC-08 模块要点

| 特性 | 说明 |
|------|------|
| 芯片 | TI CC2541, BLE 4.0 |
| 角色 | Slave（从机），手机是 Master（主机） |
| 默认波特率 | 9600 bps |
| **AT 命令** | **只能通过物理 UART 引脚发送**，BLE 透传模式下不解析 AT |
| 透传模式 | BLE 连接建立后，所有数据原样转发到 UART，不解析 |
| LED 状态 | 快闪 = 等待连接，常亮 = 已连接 |

### 2.3 关键技术栈

| 层级 | 技术 |
|------|------|
| MCU | STM32F407VET6, Cortex-M4, 168MHz, 512KB Flash, 128KB RAM |
| HAL 库 | STM32F4xx_HAL_Driver |
| RTOS | FreeRTOS v10.3.1 + CMSIS-RTOS v2 封装 |
| GUI | LVGL（小端模式） |
| 编译器 | ARMCC V5.06 (AC5) |
| IDE | Keil MDK-ARM v5.32 |
| 调试 | J-Link SWD + SEGGER RTT |
| 蓝牙 | HC-08 BLE 4.0 透传模块 |

---

## 3. Flash 分区方案

### 3.1 STM32F407VET6 物理 Sector

| Sector | 地址范围 | 大小 | 用途 |
|--------|---------|------|------|
| 0 | 0x08000000 - 0x08003FFF | 16 KB | Bootloader |
| 1 | 0x08004000 - 0x08007FFF | 16 KB | Bootloader（续） |
| 2 | 0x08008000 - 0x0800BFFF | 16 KB | **Metadata**（OTA 标志、版本、CRC） |
| 3 | 0x0800C000 - 0x0800FFFF | 16 KB | Application |
| 4 | 0x08010000 - 0x0801FFFF | 64 KB | Application |
| 5 | 0x08020000 - 0x0803FFFF | 128 KB | Application |
| 6 | 0x08040000 - 0x0805FFFF | 128 KB | Application |
| 7 | 0x08060000 - 0x0807FFFF | 128 KB | Application |

- **Bootloader**: 32 KB (Sector 0-1)，当前固件 ~438KB 已被实测验证
- **Metadata**: 16 KB (Sector 2)，存储 OTA 状态标记
- **Application**: 464 KB (Sector 3-7)，从 `0x0800C000` 开始，最大 464KB

### 3.2 分区常量

```c
#define BOOTLOADER_BASE      0x08000000
#define BOOTLOADER_SIZE      0x00008000   // 32KB
#define METADATA_BASE        0x08008000   // 16KB
#define APP_BASE             0x0800C000   // Application 起始
#define APP_MAX_SIZE         0x00074000   // 464KB
#define OTA_BOOT_MAGIC       0xB007DA7A   // "BOOT DA7A" 的谐音
```

---

## 4. OTA 通信协议

### 4.1 帧格式

```
┌──────┬──────┬──────┬──────┬──────────┬──────┐
│ 0xAA │  CMD │  LEN  │ DATA │  XOR8   │ 0x55 │
│ 1B   │  1B  │  2B   │ N B  │   1B    │  1B  │
│ 帧头  │ 命令 │ 长度  │ 数据 │  校验   │ 帧尾  │
└──────┴──────┴──────┴──────┴──────────┴──────┘
```

- **帧头/帧尾**: 定界符，用于在字节流中定位帧边界
- **LEN**: 2 字节，DATA 段的长度（不含校验和帧头帧尾）
- **XOR8**: DATA 段所有字节的异或和，逐字节 `^=` 得到

### 4.2 命令定义

| CMD | 名称 | 方向 | 说明 |
|-----|------|------|------|
| 0x01 | CMD_START | 手机→STM32 | 开始升级: [4B 文件大小][4B CRC32][2B 版本号] |
| 0x02 | CMD_DATA | 手机→STM32 | 数据块: [2B 块号][最多 240B 数据] |
| 0x03 | CMD_END | 手机→STM32 | 发送完毕: [2B 总块数] |
| 0x10 | CMD_VERSION | 手机→STM32 | 查询版本，无 payload |
| 0x80 | RSP_ACK | STM32→手机 | 确认（无 payload） |
| 0x81 | RSP_NAK | STM32→手机 | 错误: [1B 错误码] |
| 0x82 | RSP_VERSION | STM32→手机 | 版本信息: [2B 版本][4B 大小] |

### 4.3 错误码

| 码 | 含义 |
|----|------|
| 0x01 | 校验错误（请求重传当前块） |
| 0x02 | Flash 擦除失败 |
| 0x03 | Flash 写入失败 |
| 0x04 | CRC32 校验失败（整体校验不通过） |
| 0x05 | 版本回滚禁止 |
| 0x06 | 固件太大（超过 APP_MAX_SIZE） |

---

## 5. 第 1 课：蓝牙 Echo 测试 ✅

**完成日期**: 2026-07-17

### 5.1 目标

验证 HC-08 ↔ STM32 双向通信正常：手机发什么，STM32 回什么。

### 5.2 技术要点

#### HAL 中断接收模式

```c
// 启动中断接收：收到 1 字节后触发 USART2 中断
HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);

// HAL 弱定义回调 → 我们在 usart.c 中覆盖它
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        HAL_UART_Transmit(&huart2, &bt_rx_byte, 1, 100);  // 回传
        HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);     // 继续监听
    }
}
```

**关键理解**：
- `HAL_UART_Receive_IT` 是**一次性**的——收到指定字节数后自动停止
- 必须在回调里**重新调用**才能持续接收，否则只收一次
- `HAL_UART_RxCpltCallback` 在**中断上下文**中执行，不是在任务上下文
- `HAL_UART_Transmit` 是**阻塞**的——等发完或超时才返回

#### HC-08 AT vs 透传的区别

| | AT 命令模式 | 透传模式 |
|---|---|---|
| 怎么进入 | UART 物理连接（无 BLE 连接时） | BLE 连接建立后自动切换 |
| AT 命令可用？ | ✅ 可以 | ❌ 不行，数据直通 |
| LED 状态 | 快闪 | 常亮 |
| 能发 AT 的位置 | 必须通过物理 UART 引脚 | —— |

> **常见误区**：手机通过 BLE 连上 HC-08 后发送 `AT`，HC-08 不会回复 `OK`。
> 此时 HC-08 已经进入透传模式，`AT` 这两个字符直接发送到 STM32，不会解析为命令。

### 5.3 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `Core/Src/usart.c` | 波特率 115200→9600；在 `USER CODE 0` 加 Echo 回调；在 `USART2_Init 2` 启动首次中断接收 |

### 5.4 验证结果

✅ 手机蓝牙助手连接 HC-08 → 发送 "hello" → 收到 "hello" → **全链路双向通信正常**

---

## 6. 第 2 课：Bootloader 骨架 🔥

**开始日期**: 2026-07-17

### 6.1 目标

创建独立 Bootloader 工程：
1. 上电后通过 RTT 打印 Bootloader 信息
2. 检查 `0x0800C000` 是否有合法 App
3. 有 App → 跳过去执行
4. 没 App → 等待 OTA

### 6.2 核心概念讲解

#### 概念 1：为什么 Bootloader 必须是独立工程？

App 工程编译出来是一个 438KB 的固件，从 `0x08000000` 开始占用。如果 Bootloader 和 App 是同一个工程，编译器会把它们合并成一个二进制文件，无法分开烧录到不同地址。

**正确架构**：
```
Bootloader 工程（独立编译）→ 输出 pro_bootloader.hex → 烧到 0x08000000
App 工程（独立编译）      → 输出 pro.hex            → 烧到 0x0800C000
```

两个工程各有自己的 `main()`、各自的 scatter file、各自的链接地址。

#### 概念 2：Cortex-M4 上电启动流程

CPU 上电后，硬件**自动**做两件事（不需要任何代码）：

```
第 1 步：从 0x08000000+0x00 读 4 字节 → 放入 MSP（主栈指针）
第 2 步：从 0x08000000+0x04 读 4 字节 → 放入 PC（程序计数器 = 跳过去执行）
```

所以 Flash 开头的前 8 个字节就是**中断向量表的前两项**：

| 偏移 | 内容 | 示例值 |
|------|------|--------|
| 0x00 | 初始 Main Stack Pointer | 0x20010000（指向 SRAM 顶） |
| 0x04 | Reset_Handler 函数地址 | 0x0800C101（第一条指令的地址） |
| 0x08 | NMI_Handler 地址 | ... |

**boot_jump_to_app() 的本质**：手动模拟一次硬件上电——读出 App 向量表的前两个 word，设 MSP，跳 PC。

#### 概念 3：Scatter File（链接脚本）

Scatter file 告诉链接器：把代码放在 Flash 的哪个范围。

```
// Bootloader 的 scatter file → 代码限定在 32KB 内
LR_IROM1 0x08000000 0x00008000 { ... }

// App 的 scatter file → 代码从 0x0800C000 开始
LR_IROM1 0x0800C000 0x00074000 { ... }
```

同一个编译器的两个工程，同一个 STM32，但 scatter 不同 → 链接出来的地址完全不同。

#### 概念 4：VTOR（Vector Table Offset Register）

`SCB->VTOR` 是 Cortex-M4 的一个寄存器，告诉 CPU："中断向量表在哪个地址"。

- 默认值 = 0（向量表在 0x08000000）
- Bootloader 跳转前：`SCB->VTOR = 0x0800C000`（让 App 的中断能正常工作）

**为什么需要**：SysTick 来了 → CPU 查 VTOR → 跳到对应的中断处理函数。如果 VTOR 还是 0，CPU 会跳到 Bootloader 区的中断处理程序，而不会跳到 App 的。

### 6.3 任务清单

#### 任务 1：创建目录结构

```
E:\PanelPro\Bootloader\
  Inc\
  Src\
```

#### 任务 2：编写 `Bootloader/Inc/boot_config.h`

分区常量和版本号定义。对照 [第 3 节](#3-flash-分区方案) 的 Sector 分布表验证地址。

#### 任务 3：编写 `Bootloader/Src/boot_jump.c`

实现 `boot_jump_to_app(uint32_t app_addr)`：

```c
void boot_jump_to_app(uint32_t app_addr)
{
    // 1. 重定位向量表
    SCB->VTOR = app_addr;

    // 2. 恢复 App 的栈指针
    __set_MSP(*(volatile uint32_t *)app_addr);

    // 3. 跳转到 App 的 Reset_Handler
    uint32_t app_reset = *(volatile uint32_t *)(app_addr + 4);
    void (*reset_handler)(void) = (void (*)(void))app_reset;
    reset_handler();
}
```

**面试级问题**：为什么跳转前要 `__disable_irq()`？如果跳转过程中有中断来会怎样？

#### 任务 4：编写 `Bootloader/Src/boot_main.c`

最小裸机 `main()`：

1. `HAL_Init()` → `SystemClock_Config_HSI()`（16MHz，不用 PLL）
2. SysTick 初始化为 1ms 间隔（给 `HAL_Delay()` 用时基）
3. 初始化 SEGGER RTT
4. 用 `SEGGER_RTT_printf()` 打印 Bootloader 信息
5. 检查 `0x0800C000` 处第一个 word 是否在 SRAM 范围内（`0x20000000 ~ 0x20020000`）
6. 合法 → `__disable_irq()` → `boot_jump_to_app(APP_BASE)`
7. 不合法 → 死循环 + 周期性打印

**参考**：App 工程的 `Core/Src/main.c` 中 `SystemClock_Config()` 的结构，但要改为 HSI 直出。

#### 任务 5：创建 Bootloader 的 Keil 工程

##### 5.1 为什么不能直接在 App 工程上改？

App 工程用 FreeRTOS + LVGL + 一堆外设驱动，编译出来 438KB。Bootloader 只需要 HAL 裸机 + RTT，必须精简。最快的方法是从 App 工程**裁剪**出一个干净的 Bootloader 工程。

##### 5.2 第一步：复制工程文件

在 `MDK-ARM\` 目录下操作（推荐用 Windows 文件管理器）：

1. 复制 `pro.uvprojx` → 同目录粘贴，重命名为 `pro_bootloader.uvprojx`
2. 复制 `pro.uvoptx` → 同目录粘贴，重命名为 `pro_bootloader.uvoptx`

> **为什么复制 `.uvoptx`**：这个文件存的是 IDE 窗口布局、调试器设置、已打开的文件 Tab 等。不复制的话每次打开工程都要重新配置 J-Link。

##### 5.3 第二步：用 Keil 打开并删减

用 Keil MDK 打开 `pro_bootloader.uvprojx`，在左侧 **Project** 面板操作：

**整组删除（右键组名 → "Remove Group ..."）**：

| 要删除的组 | 原因 |
|-----------|------|
| `User/Tasks` | FreeRTOS 任务，Bootloader 不需要 RTOS |
| `User/GUI` | LVGL 界面代码 |
| `User/GUI_FONT` | LVGL 字体资源 |
| `User/GUI_IMAGE` | LVGL 图片资源 |
| `User/MidFunc` | 中间功能层（按键、LED、页面管理） |
| `Drivers/BSP` | 板级支持（LCD、SPI Flash 等外设驱动） |
| `Middlewares/FreeRTOS` | 整个 FreeRTOS 源码 |
| `Middlewares/LVGL` | 整个 LVGL 图形库 |

**单个文件删除（展开 `User/Core` 组，选中文件 → Delete）**：

| 要删除的文件 | 原因 |
|-------------|------|
| `freertos.c` | FreeRTOS 任务创建，Bootloader 不需要 |
| `gpio.c` | GPIO 初始化，Bootloader 用不到 LCD/按键引脚 |
| `dma.c` | DMA 配置，Bootloader 阶段不需要 DMA |
| `rtc.c` | RTC 时钟，Bootloader 不需要 |
| `spi.c` | SPI 驱动，Bootloader 不需要 |
| `tim.c` | 定时器，Bootloader 只用了 SysTick |
| `stm32f4xx_hal_timebase_tim.c` | HAL 时基（用 TIM 做），Bootloader 用 SysTick |

**保留 `User/Core` 组中的文件**（这些是 Bootloader 必需的）：

| 保留的文件 | 原因 |
|-----------|------|
| `main.c` | HAL 库生成的 `main()`，后面替换为 `boot_main.c` |
| `stm32f4xx_hal_msp.c` | HAL 底层引脚初始化（MSP = MCU Support Package） |
| `stm32f4xx_it.c` | 中断服务函数（SysTick_Handler 等） |
| `usart.c` | **重要**：USART2 初始化代码，后面 OTA 通信要用 |
| `system_stm32f4xx.c` | CMSIS 系统初始化（FPU 使能、Cache 等） |

##### 5.4 第三步：添加 Bootloader 文件

1. 右键 Target → "Add Group..." → 输入组名 `Bootloader`
2. 右键 `Bootloader` 组 → "Add Existing Files..." → 添加：
   - `..\Bootloader\Src\boot_main.c`
   - `..\Bootloader\Src\boot_jump.c`
3. 右键 `Bootloader` 组 → "Add Existing Files..." → 添加 SEGGER RTT 源文件：
   - `..\Middlewares\Third_Party\SEGGER\RTT\SEGGER_RTT.c`
   - `..\Middlewares\Third_Party\SEGGER\RTT\SEGGER_RTT_printf.c`
   - `..\Middlewares\Third_Party\SEGGER\Config\SEGGER_RTT_Conf.h`（可选，如已配好则不用）

> **重要**：Keil 的 `main.c` 是 App 的 main，和 `boot_main.c` 冲突了。两种处理方式：
> - 方式 A（推荐）：右键 `main.c` → "Options for File ..." → 勾选 "Include in Target Build" 的**对勾取消**（文件留在工程里但不参与编译）
> - 方式 B：直接从工程中 Remove `main.c`

##### 5.5 第四步：配置 Target Options

点击菜单栏 "Project" → "Options for Target ..."（或按 Alt+F7），修改以下项：

**Target 标签页**：
| 选项 | 设置 |
|------|------|
| IROM1 Start | `0x08000000` |
| IROM1 Size | `0x8000` |
| IRAM1 Start | `0x20000000` |
| IRAM1 Size | `0x20000` |

**C/C++ 标签页** → Preprocessor Symbols → Define：
```
USE_HAL_DRIVER,STM32F407xx,BOOTLOADER
```
> 追加 `BOOTLOADER` 宏。这个宏在后续开发中用来条件编译（`#ifdef BOOTLOADER`）。

**C/C++ 标签页** → Include Paths：追加以下路径（逗号分隔）：
```
..\Bootloader\Inc;..\Middlewares\Third_Party\SEGGER\RTT;..\Middlewares\Third_Party\SEGGER\Config
```

**Debug 标签页**：
- Use: `J-LINK / J-TRACE Cortex`
- Settings → Port: `SW`（SWD 接口，2 线制）

**Linker 标签页**：保持默认，勾选 "Use Memory Layout from Target Dialog"（Keil 会自动从 IROM1/IRAM1 生成 scatter file）。

##### 5.6 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 编译报 `Undefined symbol HAL_XXX` | 少加了 HAL 库的 `.c` 文件 | 检查 `Drivers/STM32F4xx_HAL_Driver` 组是否完整保留 |
| 编译报 `Undefined symbol SEGGER_RTT_printf` | RTT 源文件没加入工程 | 按 5.4 第三步添加 |
| 编译报 `Error: L6221E` (符号重名) | `main.c` 和 `boot_main.c` 都定义了 `main()` | 按 5.4 方式 A 排除 `main.c` |
| HEX 文件超过 32KB | 没有正确设置 IROM Size | 检查 Target → IROM1 Size = `0x8000` |

---

##### 🆕 5.7 新手引导：任务 5 完全指南（2026-07-19 新增）

> **阅读时间**：10 分钟。本节专为零 Keil 工程配置经验的实习生编写。

###### 5.7.1 先理解"为什么"，再动手

在打开 Keil 之前，先用大白话理解你要做什么：

**你现在有一个东西**：`pro.uvprojx`（App 工程）。这个工程编译出来是一个 438KB 的 HEX 文件，里面包含 FreeRTOS、LVGL、LCD 驱动、触摸驱动等所有代码。

**你要做一个新东西**：`pro_bootloader.uvprojx`（Bootloader 工程）。这个工程编译出来应该只有 ~5-10KB，只包含：
- HAL 库（硬件抽象层）
- 你的 `boot_main.c` 和 `boot_jump.c`
- SEGGER RTT（调试输出）
- 最基本的时钟初始化和 SysTick

**怎么做**：复制 → 删减 → 添加 → 配置。就像从一辆装满货的卡车（App 工程）拆掉所有货厢，只留驾驶室和引擎，变成一辆轻便的越野车（Bootloader 工程）。

```
App 工程 (pro.uvprojx)               Bootloader 工程 (pro_bootloader.uvprojx)
┌────────────────────────┐           ┌────────────────────┐
│ FreeRTOS (多任务)       │           │ ❌ 删掉             │
│ LVGL (图形界面)         │           │ ❌ 删掉             │
│ LCD/触摸/按键/RGB/电机  │  ──→      │ ❌ 删掉             │
│ RTC/DMA/SPI/TIM        │           │ ❌ 删掉             │
│ gpio.c / freertos.c    │           │ ❌ 删掉             │
├────────────────────────┤           ├────────────────────┤
│ HAL 库 (stm32f4xx)     │           │ ✅ 保留             │
│ usart.c / stm32f4xx_it │           │ ✅ 保留             │
│ system_stm32f4xx       │           │ ✅ 保留             │
│ main.c                 │           │ ⚠️ 替换为 boot_main  │
├────────────────────────┤           ├────────────────────┤
│                        │           │ 🆕 boot_main.c      │
│                        │           │ 🆕 boot_jump.c      │
│                        │           │ 🆕 SEGGER_RTT.c     │
└────────────────────────┘           └────────────────────┘
```

###### 5.7.2 操作前检查清单

在开始之前，确认以下内容：

| # | 检查项 | 确认 |
|---|--------|------|
| 1 | Keil MDK v5.32 已安装并能正常打开 `pro.uvprojx` | ⬜ |
| 2 | 知道 Keil 左侧 Project 面板在哪里（如果找不到：菜单 View → Project Window） | ⬜ |
| 3 | 知道怎么右键 Group 名称（如 `User/Tasks`）来 "Remove Group" | ⬜ |
| 4 | 知道怎么打开 Target Options（快捷键 Alt+F7，或菜单 Project → Options） | ⬜ |
| 5 | 已经读过一遍上面的 5.1~5.6 全部内容 | ⬜ |
| 6 | **做好备份**：把整个 `MDK-ARM` 文件夹复制一份到桌面（出错了可以恢复） | ⬜ |

###### 5.7.3 操作顺序（按这个顺序做，不要跳步）

```
第 1 步 ── 复制工程文件（Windows 文件管理器操作）
  │        MDK-ARM\pro.uvprojx  →  MDK-ARM\pro_bootloader.uvprojx
  │        MDK-ARM\pro.uvoptx   →  MDK-ARM\pro_bootloader.uvoptx
  │        验证：MDK-ARM\ 下出现了两个新文件
  │
第 2 步 ── 用 Keil 打开 pro_bootloader.uvprojx
  │        验证：Keil 标题栏显示 "pro_bootloader"
  │
第 3 步 ── 删除不需要的 Group（右键 → Remove Group）
  │        删除：User/Tasks, User/GUI, User/GUI_FONT, User/GUI_IMAGE,
  │              User/MidFunc, Drivers/BSP, Middlewares/FreeRTOS, Middlewares/LVGL
  │        验证：左侧 Project 面板只剩 User/Core, Drivers/CMSIS, Drivers/STM32F4xx_HAL_Driver
  │
第 4 步 ── 删除不需要的单文件（展开 User/Core → 选中文件 → Delete）
  │        删除：freertos.c, gpio.c, dma.c, rtc.c, spi.c, tim.c,
  │              stm32f4xx_hal_timebase_tim.c
  │        保留：main.c, stm32f4xx_hal_msp.c, stm32f4xx_it.c,
  │              usart.c, system_stm32f4xx.c
  │        验证：User/Core 组只剩 5 个文件
  │
第 5 步 ── 添加 Bootloader 组和文件
  │        ① 右键 Target → "Add Group..." → 输入 "Bootloader"
  │        ② 右键 Bootloader 组 → "Add Existing Files..." →
  │           添加 boot_main.c, boot_jump.c, SEGGER_RTT.c, SEGGER_RTT_printf.c
  │        验证：Bootloader 组下有 4 个文件
  │
第 6 步 ── 排除 main.c（右键 main.c → Options for File → 取消 Include in Target Build）
  │        验证：main.c 图标变灰或有叉号标记
  │
第 7 步 ── 配置 Target Options（Alt+F7）
  │        Target 标签页：
  │          IROM1: Start=0x08000000, Size=0x8000
  │          IRAM1: Start=0x20000000, Size=0x20000
  │        C/C++ 标签页：
  │          Define: USE_HAL_DRIVER,STM32F407xx,BOOTLOADER
  │          Include Paths: 追加 Bootloader\Inc 和 SEGGER 路径
  │        Debug 标签页：
  │          Use: J-LINK / J-TRACE Cortex → Settings → Port: SW
  │        验证：每个标签页都点过、改过、确认过
  │
第 8 步 ── 编译（F7）
  │        验证：Build Output 显示 "0 Error(s), 0 Warning(s)"
  │
  └── ✅ 完成！
```

###### 5.7.4 最容易犯的 5 个错误

| # | 错误 | 为什么容易犯 | 正确做法 |
|---|------|-------------|---------|
| 1 | **忘了排除 main.c** | 和 boot_main.c 在同一个 Target 里，链接器发现两个 `main()` 函数 | 右键 main.c → Options → 取消 Include in Target Build 的勾 |
| 2 | **忘了添加 SEGGER_RTT_printf.c** | 只加了 `SEGGER_RTT.c`，但 `boot_main.c` 调用了 `SEGGER_RTT_printf()` | 两个都要加：`SEGGER_RTT.c` + `SEGGER_RTT_printf.c` |
| 3 | **Include Paths 路径写错** | Keil 的 Include Paths 是相对于 `.uvprojx` 文件所在目录（即 `MDK-ARM\`）的 | 从 `MDK-ARM\` 出发：`..\Bootloader\Inc`（注意是两个点） |
| 4 | **IRAM1 Size 写成 0x20000 但 IRAM1 单位是字节** | 0x20000 = 128KB = STM32F407 全部 SRAM，正确 | ✅ 这个没问题，但要理解 128KB = 0x20000 |
| 5 | **IROM1 Size 写成 0x80000（512KB，整个 Flash）** | 习惯性按 App 工程的大小填 | Bootloader 只能占 32KB → `0x8000` |

###### 5.7.5 编译报错了怎么办

不要慌，按这个顺序排查：

```
1. 看第一个 Error（后面的 Error 可能是连锁反应，修好第一个可能全没）
2. 对着上面的"最容易犯的 5 个错误"检查
3. 如果是 "Undefined symbol XXX"：
   → 说明某个函数/变量找不到定义
   → 检查对应的 .c 文件是否在工程里
4. 如果是 "L6221E"（链接错误，符号重名）：
   → 几乎肯定是 main.c 没排除
5. 如果还是解决不了：
   → 截图 Build Output 完整输出，发给导师
```

###### 5.7.6 任务 5 做完的标志

不要猜自己"应该好了"。确认以下每一项：

- [ ] `pro_bootloader.uvprojx` 存在且能用 Keil 打开
- [ ] 左侧 Project 面板结构与文档描述一致
- [ ] Target Options 三个标签页都配置正确
- [ ] 按 F7 编译，**0 Error, 0 Warning**
- [ ] 编译输出中看到 `"pro_bootloader.hex"` 文件生成路径
- [ ] 把编译输出的完整内容截图发给导师

#### 任务 6：编译并验证

1. 编译，修掉所有编译错误
2. Keil Download 烧录 Bootloader（注意：会擦除整个 Flash，App 暂时不跑）
3. 打开 J-Link RTT Viewer
4. 复位 STM32
5. **期望输出**：
   ```
   [BOOT] ==============================
   [BOOT] PanelPro Bootloader v1.0
   [BOOT] Flash: 32KB | App @ 0x0800C000
   [BOOT] ==============================
   [BOOT] No valid App found! Stay in bootloader.
   ```

### 6.4 Code Review — 导师反馈 (2026-07-19)

以下 3 个 Bug 必须在任务 5 之前修复：

#### Bug 1：`boot_jump.c:3` — 多余的 `extern` 声明

```c
// ❌ 错误：在定义函数的同一个 .c 文件中 extern 声明自己
extern void boot_jump_to_app(uint32_t app_addr);

// ✅ 正确：删掉这一行。extern 用于引用其他 .c 文件的函数，
//     你已经在下面定义了它，不需要前向声明。
```

**知识点**：`extern` 告诉编译器"这个函数在别处定义"。但你在同一个文件里定义它，所以这个声明不仅多余，还会让读代码的人困惑。

#### Bug 2：`boot_main.c:28` — 打印信息逻辑反了

```c
// ❌ 错误：进入了"App 有效"的分支，却打印 "APP invalid"
SEGGER_RTT_printf(0,"APP invalid, jump to app\r\n");

// ✅ 正确：
SEGGER_RTT_printf(0,"App valid, jumping to 0x%08X\r\n", APP_BASE);
```

#### Bug 3：`boot_main.c:37` — 拼写错误

```c
// ❌ "vaLid" → L 不应该大写
SEGGER_RTT_printf(0,"No vaLid APP,waiting for OTA\r\n");

// ✅ 正确：
SEGGER_RTT_printf(0,"No valid App found! Waiting for OTA...\r\n");
```

#### 其他建议（不强制）

`boot_main.c:29` 的 `__disable_irq()` 与 `boot_jump.c:13` 重复了——`boot_jump_to_app()` 内部已经关中断。保留或删除都可以，但建议删掉 `boot_main.c` 中的那行，让跳转函数自己对关中断负责（单一职责原则）。

---

### 6.5 检查点

| # | 检查项 | 完成 |
|---|--------|------|
| 1 | 创建 `Bootloader/Inc/` 和 `Bootloader/Src/` 目录 | ✅ |
| 2 | `boot_config.h` 地址计算正确 | ✅ |
| 3 | `boot_jump.c` 跳转逻辑正确 | ✅ |
| 4 | `boot_main.c` 能编译通过 | ✅ |
| 5 | Bootloader 工程编译 0 错误 | ✅ |
| 6 | RTT 输出 "No valid App found" | ✅ |

---

## 7. 第 3 课：App 工程重定位 ✅

**完成日期**: 2026-07-19

**前置条件**: 第 2 课全部完成（Bootloader 能独立编译 + RTT 验证通过）

### 7.1 目标

让现有的 App 工程从 `0x0800C000` 开始运行，给 Bootloader 腾出前面 48KB 空间。

### 7.2 为什么需要重定位 App？

目前 App 工程的 Flash 起始地址是 `0x08000000`（STM32 默认）。但现在 Bootloader 占用了 `0x08000000 ~ 0x08007FFF`（32KB），Metadata 占用了 `0x08008000 ~ 0x0800BFFF`（16KB），App 必须搬到 `0x0800C000`。

**在 STM32 上，App 和 Bootloader 是两个独立工程**，它们通过 Flash 地址隔离：

```
烧录后 Flash 布局:
┌──────────────────────┐ 0x08000000
│   Bootloader (32KB)  │  ← pro_bootloader.hex 烧录
├──────────────────────┤ 0x08008000
│   Metadata  (16KB)   │  ← 运行时写入，不随 HEX 烧录
├──────────────────────┤ 0x0800C000
│                      │
│   Application        │  ← pro.hex 烧录
│   (最大 464KB)       │
│                      │
└──────────────────────┘ 0x08080000 (512KB Flash 末尾)
```

> **关键理解**：烧录 App 的 HEX 文件时，**不会覆盖 Bootloader**。因为 App HEX 的地址从 `0x0800C000` 开始，Keil/J-Link 只擦写这个范围。这和"两个 HEX 拼接成一个再烧录"不同。

### 7.3 修改内容

App 工程只需要改 **3 个地方**：

#### 修改 1：Linker — 链接起始地址

Keil → Project → Options for Target → Target 标签页：

```diff
- IROM1 Start: 0x08000000  Size: 0x80000
+ IROM1 Start: 0x0800C000  Size: 0x74000
```

这会改变 scatter file，让链接器把**所有代码**定位到 `0x0800C000` 之后。验证方法：编译后在 `.map` 文件中搜索 `ER_IROM1`，确认 Base 是 `0x0800c000`。

#### 修改 2：C/C++ — 宏定义追加 APP_BASE

C/C++ → Define：

```
USE_HAL_DRIVER,STM32F407xx,APP_BASE=0x0800C000
```

> Keil 的 `-D` 语法支持 `-DAPP_BASE=0x0800C000`，编译时等价于写 `#define APP_BASE 0x0800C000`。

#### 修改 3：`system_stm32f4xx.c` — 修改 VTOR 默认值

```c
// 文件: Core/Src/system_stm32f4xx.c
// 找到 SystemInit() 函数，修改 SCB->VTOR 设置：

#ifdef APP_BASE
    SCB->VTOR = APP_BASE;           // App 运行时向量表在 0x0800C000
#else
    SCB->VTOR = FLASH_BASE;         // 默认 0x08000000（向后兼容）
#endif
```

> **为什么需要**：STM32 上电后从 `0x08000000` 启动（Bootloader），Bootloader 跳转到 App。App 的 SysTick、USART 等中断来了 → CPU 查 VTOR → 跳到正确的 ISR。如果 VTOR 没改，中断会跳回 Bootloader 的中断处理函数。

### 7.4 验证方法

1. 先烧录 Bootloader（`pro_bootloader.hex`，如果有的话）
2. 再烧录 App（`pro.hex`）→ **注意**：Keil Download 时确认不要勾选 "Erase Full Chip"，选 "Erase Sectors" 只擦写 App 区域
3. 打开 J-Link RTT Viewer
4. 按复位键
5. **期望输出**：
   ```
   [BOOT] App valid, jumping to 0x0800C000
   ```
   然后 App 正常启动，LVGL 界面显示，蓝牙 Echo 正常工作。

### 7.5 常见问题

| 现象 | 诊断方法 | 可能原因 | 解决 |
|------|---------|---------|------|
| 烧录后 MCU 完全不跑 | RTT Viewer 无任何输出 | Bootloader 还没烧录，或 HEX 覆盖了 Bootloader | 先烧 Bootloader，再烧 App |
| App 启动后崩溃 (HardFault) | 在 `HardFault_Handler` 打断点 | `SCB->VTOR` 没改，中断向量表位置不对 | 检查修改 3 |
| LVGL 界面不显示 | 检查 SPI/LCD 初始化是否通过 | 链接地址改了对，但某个外设中断跑飞了 | 检查 Tick Timer 中断是否正常 |
| 编译出的 HEX 地址还是从 0x08000000 | 打开 .map 文件看 Memory Map | IROM1 改错了或没保存 | 重新检查 Target → IROM1 |

---

## 8. 第 4 课：Flash 操作 + 协议解析 🔥

**开始日期**: 2026-07-19

### 8.1 目标

Bootloader 能：
1. 通过蓝牙接收固件帧（在第 1 课 Echo 基础上改为帧解析）
2. 解析帧 → 提取固件数据
3. 把数据写入 Flash 的 App 区域（Sector 3-7）
4. 全部写入后校验 CRC32，标记 OTA 完成

### 8.2 学习路线图

```
任务 1: UART 环形缓冲区     ──→  任务 2: 帧解析状态机
（数据不丢）                      （识别完整帧）
                                        │
                                        ▼
任务 3: Flash 擦除/写入     ←──  任务 4: OTA 完整流程串联
（操作硬件）                      （手机 → 蓝牙 → Flash → 校验）
```

> 任务 1 和 2 可以并行思考，但建议按顺序做——环形缓冲没调通之前，帧解析没法验证。

---

### 8.3 任务 1：UART 环形缓冲区

#### 8.3.1 为什么要升级？

回顾第 1 课的 Echo 实现：

```c
// 当前做法：每次只收 1 个字节
uint8_t bt_rx_byte;
HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    HAL_UART_Transmit(&huart2, &bt_rx_byte, 1, 100);  // Echo
    HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);     // 重新监听
}
```

**问题**：一帧 OTA 数据最多 246 字节（`0xAA + CMD + LEN + DATA + XOR8 + 0x55`）。如果每收 1 字节就进一次中断、调一次回调，246 字节 = 246 次中断。而且每次回调里还做 `HAL_UART_Transmit`（阻塞）——OTA 模式下根本来不及。

**升级方案**：中断只负责把字节**存入环形缓冲区**，主循环负责**从缓冲区取数据分析**。各干各的，互不阻塞。

```
中断上下文（快速进出）              主循环（慢慢处理）
┌──────────────────────┐      ┌──────────────────────┐
│ USART2 中断来了       │      │ while(1) {           │
│ 读 DR 寄存器 → 1 字节 │      │   if(缓冲区有数据)    │
│ 写入环形缓冲区        │ ───→ │     喂给帧解析器      │
│ 清中断标志，走人      │      │ }                    │
└──────────────────────┘      └──────────────────────┘
```

#### 8.3.2 环形缓冲区原理

想象一个固定大小的数组 + 两个指针：

```
数组: [ _ ][ _ ][ _ ][ _ ][ _ ][ _ ][ _ ][ _ ]   (大小 = 256)
         ↑                    ↑
       head(=0)             tail(=0)

写入 5 个字节后:
       [ A ][ B ][ C ][ D ][ E ][ _ ][ _ ][ _ ]
         ↑                    ↑
       head(=0)             tail(=5)

读出 3 个字节后:
       [ _ ][ _ ][ _ ][ D ][ E ][ _ ][ _ ][ _ ]
                       ↑          ↑
                     head(=3)   tail(=5)

tail 跑到数组末尾时:   tail = (tail + 1) % BUFFER_SIZE   → 回到 0
```

| 变量 | 含义 | 谁写 |
|------|------|------|
| `buffer[]` | 存储空间 | 中断写入 |
| `head` | 下一个要读的位置 | 主循环更新 |
| `tail` | 下一个要写的位置 | 中断更新 |

**关键公式**：
```c
// 缓冲区是否为空？（head == tail 表示空）
#define RING_EMPTY(rb)  ((rb).head == (rb).tail)

// 缓冲区还剩多少空间？
#define RING_FREE(rb)   ((RING_SIZE - 1) - ((rb).tail - (rb).head + RING_SIZE) % RING_SIZE)

// tail 前进 1 格
rb.tail = (rb.tail + 1) % RING_SIZE;

// head 前进 1 格
rb.head = (rb.head + 1) % RING_SIZE;
```

> ⚠️ **惯例**：用 `(head == tail)` 判定空，所以实际可用空间 = `RING_SIZE - 1`。256 字节的数组存 255 字节就满了。这是为了避免 `head == tail` 同时表示"空"和"满"。

#### 8.3.3 创建文件

新建 `Bootloader/Inc/ring_buffer.h`：

```c
#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>

#define RING_SIZE   512     // 环形缓冲区大小（OTA 帧最大 246 字节，512 足够了）

typedef struct {
    uint8_t buffer[RING_SIZE];
    volatile uint16_t head;   // 读指针（主循环改）
    volatile uint16_t tail;   // 写指针（中断改）
} RingBuffer_t;

void     RingBuffer_Init(RingBuffer_t *rb);
uint8_t  RingBuffer_Put(RingBuffer_t *rb, uint8_t byte);   // 返回 1=成功, 0=满了
uint8_t  RingBuffer_Get(RingBuffer_t *rb, uint8_t *byte);  // 返回 1=有数据, 0=空的
uint16_t RingBuffer_Available(RingBuffer_t *rb);           // 返回可读字节数
void     RingBuffer_Flush(RingBuffer_t *rb);               // 清空

#endif
```

新建 `Bootloader/Src/ring_buffer.c`：

```c
#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    for (int i = 0; i < RING_SIZE; i++) rb->buffer[i] = 0;
}

uint8_t RingBuffer_Put(RingBuffer_t *rb, uint8_t byte)
{
    uint16_t next_tail = (rb->tail + 1) % RING_SIZE;
    if (next_tail == rb->head)   // 满
        return 0;
    
    rb->buffer[rb->tail] = byte;
    rb->tail = next_tail;
    return 1;
}

uint8_t RingBuffer_Get(RingBuffer_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail)    // 空
        return 0;
    
    *byte = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % RING_SIZE;
    return 1;
}

uint16_t RingBuffer_Available(RingBuffer_t *rb)
{
    return (rb->tail - rb->head + RING_SIZE) % RING_SIZE;
}

void RingBuffer_Flush(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}
```

#### 8.3.4 改造 USART2 中断

在 Bootloader 工程中修改 `Core/Src/usart.c`（或新建一个 USART 初始化文件），核心改动：

```c
#include "ring_buffer.h"

RingBuffer_t bt_rx_ring;   // 全局环形缓冲

// USART2 初始化（和 App 的 usart.c 类似，但精简）
void BT_UART_Init(void)
{
    // 使能 USART2 时钟、配置 GPIO PA3(RX) PD5(TX)、
    // 波特率 9600、8N1、使能 RXNE 中断
    // ...（这部分可以用 HAL 生成的代码）
    
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);  // 开启接收中断（非空）
}

// 改造 HAL 回调：不再是 Echo，而是存入环形缓冲
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // 不在这里写了，改用 RXNE 中断方式
}
```

**但实际上**，HAL 的 `HAL_UART_Receive_IT` 不适合持续流式接收。更好的做法是直接操作寄存器：

```c
// USART2 中断服务函数（在 stm32f4xx_it.c 中的 USART2_IRQHandler）
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
    {
        uint8_t byte = (uint8_t)(huart2.Instance->DR & 0xFF);
        RingBuffer_Put(&bt_rx_ring, byte);
        __HAL_UART_CLEAR_FLAG(&huart2, UART_FLAG_RXNE);
    }
    
    // 其他 USART2 中断标志...
}
```

> 💡 **导师提示**：如果你觉得直接改 `stm32f4xx_it.c` 里的中断函数有风险，可以先在目前 App 工程的 Echo 框架上验证环形缓冲逻辑——收到字节后 `RingBuffer_Put`，主循环 `RingBuffer_Get` 再 `HAL_UART_Transmit` 发回去。逻辑一样，只是先不拆中断。

#### 8.3.5 验证方法

在 `boot_main.c` 的主循环中：

```c
// 在第 4 课开发期间，暂时注释掉 App 跳转逻辑
// 让 Bootloader 变成：收蓝牙数据 → 打印帧信息
while(1)
{
    uint8_t byte;
    while (RingBuffer_Get(&bt_rx_ring, &byte))
    {
        SEGGER_RTT_printf(0, "%02X ", byte);  // 打印每个字节的十六进制
    }
}
```

用手机蓝牙助手发送 `AA 01 00 00 55`（一个最小帧），RTT 应该显示 `AA 01 00 00 55`——不丢字节，不错位。

#### 8.3.6 检查点

| # | 检查项 | 完成 |
|---|--------|------|
| 1 | `ring_buffer.c/.h` 编写完成，编译 0 错误 | ⬜ |
| 2 | USART2 中断改为存入环形缓冲 | ⬜ |
| 3 | 手机发数据 → RTT 逐字节打印，无丢失 | ⬜ |

---

### 8.4 任务 2：帧解析状态机

#### 8.4.1 从字节流到完整帧

环形缓冲给你的是字节流。帧解析器的工作：从字节流中找到 `0xAA...0x55` 包裹的完整帧。

```
字节流: ...xx xx AA 02 00 F0 [240 bytes data] C5 55 xx xx AA 80 00 00 80 55...
                        ↑                            ↑
                     帧 1：DATA 块                   帧 2：ACK
```

#### 8.4.2 状态机设计

```
            byte != 0xAA
    ┌──────────────────────────────┐
    │                              │
    ▼                              │
┌───────┐  byte==0xAA  ┌──────────┐│
│ WAIT  │─────────────→│  GOT_AA  ││
│_HEADER│              │          ││
└───────┘              └────┬─────┘│
                            │      │
                     byte是合法CMD?
                         ┌──┴──┐
                         │     │ 否 → 回 WAIT_HEADER
                         │ 是  │
                         ▼     │
                    ┌──────────┐│
                    │ GOT_CMD  ││
                    │ 保存 CMD ││
                    └────┬─────┘│
                         │      │
                    读 LEN (2字节)
                         │
                         ▼
                    ┌──────────┐
                    │ GOT_LEN  │
                    │ 保存 LEN │
                    └────┬─────┘
                         │
                    读 LEN 字节 DATA
                         │
                         ▼
                    ┌──────────┐
                    │ GOT_DATA │
                    │ 保存所有 │
                    │ 数据字节  │
                    └────┬─────┘
                         │
                    读 1 字节 XOR8
                         │
                         ▼
                    ┌──────────────┐
                    │ 计算 XOR8     │
                    │ 与实际值对比   │
                    └──┬───────────┘
                       │
                  ┌────┴────┐
                  │         │
               匹配       不匹配
                  │         │
                  ▼         ▼
              读 1 字节  回 WAIT_HEADER
              byte==0x55?  (帧损坏，丢弃)
                  │
             ┌────┴────┐
             │         │
            是        否
             │         │
             ▼         ▼
        ┌────────┐  回 WAIT_HEADER
        │ 帧完整! │
        │ 处理帧  │
        └────────┘
             │
             ▼
         WAIT_HEADER
```

#### 8.4.3 数据结构

新建 `Bootloader/Inc/ota_protocol.h`：

```c
#ifndef __OTA_PROTOCOL_H__
#define __OTA_PROTOCOL_H__

#include <stdint.h>

/* ── 帧格式常量 ── */
#define FRAME_HEADER    0xAA
#define FRAME_TRAILER   0x55
#define FRAME_DATA_MAX  240     // DATA 段最大 240 字节

/* ── 命令码 ── */
#define CMD_START       0x01
#define CMD_DATA        0x02
#define CMD_END         0x03
#define CMD_VERSION     0x10
#define RSP_ACK         0x80
#define RSP_NAK         0x81
#define RSP_VERSION     0x82

/* ── 错误码 ── */
#define ERR_CHECKSUM    0x01
#define ERR_FLASH_ERASE 0x02
#define ERR_FLASH_WRITE 0x03
#define ERR_CRC32       0x04
#define ERR_ROLLBACK    0x05
#define ERR_TOO_LARGE   0x06

/* ── 解析状态机 ── */
typedef enum {
    STATE_WAIT_HEADER = 0,   // 等待 0xAA
    STATE_GOT_HEADER,        // 收到 0xAA，等待 CMD
    STATE_GOT_CMD,           // 收到 CMD，等待 LEN
    STATE_GOT_LEN,           // 收到 LEN，等待 DATA
    STATE_GOT_DATA,          // 收到所有 DATA，等待校验
    STATE_GOT_XOR,           // 收到 XOR，等待 0x55
} FrameState_t;

/* ── 解析上下文 ── */
typedef struct {
    FrameState_t state;
    uint8_t  cmd;
    uint16_t data_len;
    uint8_t  data[FRAME_DATA_MAX];
    uint16_t data_idx;       // 当前已收 DATA 字节数
    uint8_t  received_xor;   // 收到的 XOR8
    uint8_t  calculated_xor; // 自己算的 XOR8
} FrameParser_t;

/* ── 一帧解析结果 ── */
typedef struct {
    uint8_t  cmd;
    uint16_t data_len;
    uint8_t  data[FRAME_DATA_MAX];
} OtaFrame_t;

/* ── API ── */
void FrameParser_Init(FrameParser_t *fp);
int  FrameParser_Feed(FrameParser_t *fp, uint8_t byte, OtaFrame_t *out);
//  返回:  0 = 还未收到完整帧
//         1 = 收到完整帧（out 有效）
//        -1 = 校验失败，帧丢弃

#endif
```

#### 8.4.4 状态机实现

新建 `Bootloader/Src/ota_protocol.c`：

```c
#include "ota_protocol.h"
#include <string.h>

void FrameParser_Init(FrameParser_t *fp)
{
    fp->state = STATE_WAIT_HEADER;
    fp->data_idx = 0;
    fp->calculated_xor = 0;
    memset(fp->data, 0, sizeof(fp->data));
}

// 计算 XOR8（逐字节异或）
static uint8_t calc_xor8(const uint8_t *data, uint16_t len)
{
    uint8_t x = 0;
    for (uint16_t i = 0; i < len; i++)
        x ^= data[i];
    return x;
}

int FrameParser_Feed(FrameParser_t *fp, uint8_t byte, OtaFrame_t *out)
{
    switch (fp->state)
    {
    case STATE_WAIT_HEADER:
        if (byte == FRAME_HEADER)
        {
            fp->state = STATE_GOT_HEADER;
            fp->data_idx = 0;
            fp->calculated_xor = 0;
        }
        break;

    case STATE_GOT_HEADER:
        // 验证 CMD 是否合法
        if (byte == CMD_START || byte == CMD_DATA || byte == CMD_END ||
            byte == CMD_VERSION || byte == RSP_ACK || byte == RSP_NAK ||
            byte == RSP_VERSION)
        {
            fp->cmd = byte;
            fp->state = STATE_GOT_CMD;
        }
        else
        {
            fp->state = STATE_WAIT_HEADER;  // 非法 CMD，回初始状态
        }
        break;

    case STATE_GOT_CMD:
        // 读 LEN 低字节
        fp->data_len = byte;
        fp->state = STATE_GOT_LEN;
        break;

    case STATE_GOT_LEN:
        // 读 LEN 高字节
        fp->data_len |= (byte << 8);
        
        if (fp->data_len > FRAME_DATA_MAX)
        {
            fp->state = STATE_WAIT_HEADER;  // 长度非法，丢弃
        }
        else if (fp->data_len == 0)
        {
            fp->state = STATE_GOT_DATA;     // 无数据帧（如 ACK），直接跳到校验
        }
        else
        {
            fp->state = STATE_GOT_DATA;
        }
        break;

    case STATE_GOT_DATA:
        fp->data[fp->data_idx++] = byte;
        if (fp->data_idx >= fp->data_len)
        {
            fp->state = STATE_GOT_XOR;      // 收完 DATA，准备收 XOR
        }
        break;

    case STATE_GOT_XOR:
        fp->received_xor = byte;
        fp->calculated_xor = calc_xor8(fp->data, fp->data_len);
        fp->state = STATE_WAIT_HEADER;      // 准备收帧尾
        
        if (fp->received_xor != fp->calculated_xor)
        {
            return -1;  // XOR 校验失败，帧丢弃
        }
        
        // 校验通过！输出完整帧
        out->cmd = fp->cmd;
        out->data_len = fp->data_len;
        memcpy(out->data, fp->data, fp->data_len);
        return 1;       // 完整帧

    default:
        fp->state = STATE_WAIT_HEADER;
        break;
    }
    
    return 0;  // 还没收完
}
```

> ⚠️ **注意**：当前实现省略了帧尾 `0x55` 的检查（`GOT_XOR` 后直接判断校验 → 输出帧）。可以在后续加上 `STATE_GOT_TRAILER` 状态——`GOT_XOR` 验证成功后 → 等待 `0x55` → 确认帧尾。两种方式都行，影响不大。

#### 8.4.5 验证方法

在主循环中串联环形缓冲和帧解析器：

```c
FrameParser_t parser;
FrameParser_Init(&parser);

while(1)
{
    uint8_t byte;
    while (RingBuffer_Get(&bt_rx_ring, &byte))
    {
        OtaFrame_t frame;
        int result = FrameParser_Feed(&parser, byte, &frame);
        
        if (result == 1)
        {
            SEGGER_RTT_printf(0, "[FRAME] CMD:0x%02X LEN:%d\r\n",
                              frame.cmd, frame.data_len);
            // 打印前 16 字节数据
            SEGGER_RTT_printf(0, "[DATA] ");
            for (int i = 0; i < frame.data_len && i < 16; i++)
                SEGGER_RTT_printf(0, "%02X ", frame.data[i]);
            SEGGER_RTT_printf(0, "\r\n");
        }
        else if (result == -1)
        {
            SEGGER_RTT_printf(0, "[ERR] XOR8 mismatch!\r\n");
        }
    }
}
```

手机蓝牙助手发送测试帧：

| 发送（十六进制） | 含义 | 期望 RTT 输出 |
|-----------------|------|--------------|
| `AA 10 00 00 10 55` | 版本查询 | `[FRAME] CMD:0x10 LEN:0` |
| `AA 80 00 00 80 55` | ACK | `[FRAME] CMD:0x80 LEN:0` |
| `AA 02 00 05 01 02 03 04 05 01 55` | DATA 块，5 字节 | `[FRAME] CMD:0x02 LEN:5` + `[DATA] 01 02 03 04 05` |

#### 8.4.6 检查点

| # | 检查项 | 完成 |
|---|--------|------|
| 1 | `ota_protocol.c/.h` 编写完成，编译 0 错误 | ⬜ |
| 2 | 发送版本查询帧 → RTT 正确识别 `CMD:0x10` | ⬜ |
| 3 | 发送带 payload 的 DATA 帧 → RTT 正确打印数据 | ⬜ |
| 4 | 发送错误 XOR8 的帧 → RTT 打印 XOR8 mismatch | ⬜ |

---

### 8.5 任务 3：Flash 擦除与写入

#### 8.5.1 STM32F4 Flash 操作要点

| 操作 | HAL 函数 | 关键限制 |
|------|---------|---------|
| 解锁 | `HAL_FLASH_Unlock()` | 写 Flash 前必须解锁（写保护寄存器） |
| 擦除 | `FLASH_Erase_Sector(sector, VOLTAGE_RANGE_3)` | **必须先擦后写**；擦除以 Sector 为单位 |
| 写入 | `HAL_FLASH_Program(TYPEPROGRAM_BYTE, addr, data)` | 支持 Byte/HalfWord/Word/QuadWord |
| 上锁 | `HAL_FLASH_Lock()` | 操作完上锁防止误写 |

**核心认知**：Flash 只能从 1 变 0（写操作），不能从 0 变 1（擦除操作）。擦除会把整个 Sector 全部变回 0xFF。

#### 8.5.2 Sector 地址对照表

```c
// Sector 号 ↔ 地址的映射（ST 手册规定，不可改）
uint32_t sector_start_addr(uint8_t sector)
{
    static const uint32_t table[] = {
        0x08000000,  // Sector 0  (16KB) ← Bootloader
        0x08004000,  // Sector 1  (16KB)
        0x08008000,  // Sector 2  (16KB) ← Metadata
        0x0800C000,  // Sector 3  (16KB) ← App 开始
        0x08010000,  // Sector 4  (64KB)
        0x08020000,  // Sector 5  (128KB)
        0x08040000,  // Sector 6  (128KB)
        0x08060000,  // Sector 7  (128KB)
    };
    if (sector > 7) return 0;
    return table[sector];
}
```

#### 8.5.3 新建 Flash 操作模块

新建 `Bootloader/Inc/flash_ops.h`：

```c
#ifndef __FLASH_OPS_H__
#define __FLASH_OPS_H__

#include <stdint.h>

int  Flash_Erase_App_Area(void);                          // 擦除 Sector 3-7, 返回 0=成功
int  Flash_Erase_Sector(uint8_t sector);                  // 擦除单个 Sector
int  Flash_Write(uint32_t addr, const uint8_t *data, uint32_t len); // 按字节写入
void Flash_Test(void);                                     // 自测：写→读→验证

#endif
```

新建 `Bootloader/Src/flash_ops.c`：

```c
#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include "boot_config.h"

// 擦除 App 区域所有 Sector (Sector 3 ~ 7)
int Flash_Erase_App_Area(void)
{
    HAL_FLASH_Unlock();
    
    for (uint8_t s = 3; s <= 7; s++)
    {
        FLASH_EraseInitTypeDef erase = {
            .TypeErase    = FLASH_TYPEERASE_SECTORS,
            .Sector       = s,
            .NbSectors    = 1,
            .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        };
        
        uint32_t sector_error = 0;
        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;   // 擦除失败
        }
    }
    
    HAL_FLASH_Lock();
    return 0;
}

// 擦除单个 Sector
int Flash_Erase_Sector(uint8_t sector)
{
    if (sector > 7) return -1;
    
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = sector,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    
    uint32_t sector_error = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);
    
    HAL_FLASH_Lock();
    return (status == HAL_OK) ? 0 : -1;
}

// 按字节顺序写入 Flash
// 注意：Flash 写入必须以 Word(4字节) 为单位，这里封装成按字节写入
int Flash_Write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    HAL_FLASH_Unlock();
    
    for (uint32_t i = 0; i < len; i++)
    {
        HAL_StatusTypeDef status;
        
        // HAL 的 PROGRAM_BYTE 一次写 1 字节
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]);
        
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
        
        // 验证写入
        if (*(volatile uint8_t *)(addr + i) != data[i])
        {
            HAL_FLASH_Lock();
            return -2;  // 写入验证失败
        }
    }
    
    HAL_FLASH_Lock();
    return 0;
}
```

> ⚠️ **注意**：`HAL_FLASH_Program` 按字节写效率很低（每次写 1 字节都要等 Flash 控制器完成）。生产级代码会用 Word/DoubleWord 批量写入。先跑通功能，优化是后面的事。

#### 8.5.4 验证方法

在 `boot_main.c` 中加一段自测：

```c
// 自测：擦除 Sector 3 的第一个 Page → 写几个字节 → 读回来对比
void flash_self_test(void)
{
    SEGGER_RTT_printf(0, "[FLASH TEST] Erasing Sector 3...\r\n");
    
    if (Flash_Erase_Sector(3) != 0)
    {
        SEGGER_RTT_printf(0, "[FLASH TEST] ERASE FAILED!\r\n");
        return;
    }
    
    // 验证擦除后全是 0xFF
    uint8_t first_byte = *(volatile uint8_t *)APP_BASE;
    SEGGER_RTT_printf(0, "[FLASH TEST] After erase, APP_BASE[0] = 0x%02X (expect 0xFF)\r\n", first_byte);
    
    // 写入测试数据
    uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    if (Flash_Write(APP_BASE, test_data, 4) != 0)
    {
        SEGGER_RTT_printf(0, "[FLASH TEST] WRITE FAILED!\r\n");
        return;
    }
    
    // 读回验证
    SEGGER_RTT_printf(0, "[FLASH TEST] Read back: %02X %02X %02X %02X\r\n",
                      *(volatile uint8_t *)(APP_BASE + 0),
                      *(volatile uint8_t *)(APP_BASE + 1),
                      *(volatile uint8_t *)(APP_BASE + 2),
                      *(volatile uint8_t *)(APP_BASE + 3));
    
    SEGGER_RTT_printf(0, "[FLASH TEST] PASS!\r\n");
}
```

编译烧录后 RTT 期望输出：
```
[FLASH TEST] Erasing Sector 3...
[FLASH TEST] After erase, APP_BASE[0] = 0xFF (expect 0xFF)
[FLASH TEST] Read back: DE AD BE EF
[FLASH TEST] PASS!
```

#### 8.5.5 检查点

| # | 检查项 | 完成 |
|---|--------|------|
| 1 | `flash_ops.c/.h` 编译 0 错误 | ⬜ |
| 2 | Sector 3 擦除成功率 100%（连续测 3 次） | ⬜ |
| 3 | 写入 + 回读验证通过 | ⬜ |

---

### 8.6 任务 4：OTA 完整流程串联

#### 8.6.1 接收流程

```
手机                          Bootloader
 │                                │
 │── CMD_START [size][crc][ver] ─→│  ① 验证 size ≤ 464KB
 │                                │  ② 擦除 Sector 3-7
 │←── RSP_ACK ────────────────────│
 │                                │
 │── CMD_DATA [blk=0][240B] ─────→│  ③ 写入 APP_BASE + 0
 │←── RSP_ACK ────────────────────│
 │── CMD_DATA [blk=1][240B] ─────→│  ④ 写入 APP_BASE + 240
 │←── RSP_ACK ────────────────────│
 │         ... (N 帧)              │
 │                                │
 │── CMD_END [total_blks] ───────→│  ⑤ 计算写入数据的 CRC32
 │                                │  ⑥ 与 CMD_START 中的 CRC 比对
 │←── RSP_ACK / RSP_NAK(CRC错误) ─│
 │                                │
 │     升级完成！                  │
```

#### 8.6.2 修改 `boot_main.c` 主循环

```c
// OTA 全局状态
typedef enum {
    OTA_IDLE = 0,
    OTA_RECEIVING,
    OTA_COMPLETE,
    OTA_ERROR,
} OtaState_t;

OtaState_t ota_state = OTA_IDLE;
uint32_t   ota_total_size = 0;
uint32_t   ota_crc32_expected = 0;   // 手机告知的 CRC32
uint32_t   ota_write_addr = APP_BASE;
uint16_t   ota_block_count = 0;

void handle_ota_frame(OtaFrame_t *frame)
{
    switch (frame->cmd)
    {
    case CMD_START:
    {
        // 解析 payload: [4B size][4B crc32][2B version]
        uint32_t fw_size = *(uint32_t *)(&frame->data[0]);
        uint32_t fw_crc  = *(uint32_t *)(&frame->data[4]);
        // uint16_t fw_ver  = *(uint16_t *)(&frame->data[8]);
        
        if (fw_size > APP_SIZE)
        {
            send_nak(ERR_TOO_LARGE);
            ota_state = OTA_ERROR;
            return;
        }
        
        SEGGER_RTT_printf(0, "[OTA] START: size=%d bytes, CRC=0x%08X\r\n",
                          fw_size, fw_crc);
        SEGGER_RTT_printf(0, "[OTA] Erasing App area...\r\n");
        
        if (Flash_Erase_App_Area() != 0)
        {
            send_nak(ERR_FLASH_ERASE);
            ota_state = OTA_ERROR;
            return;
        }
        
        ota_total_size = fw_size;
        ota_crc32_expected = fw_crc;
        ota_write_addr = APP_BASE;
        ota_block_count = 0;
        ota_state = OTA_RECEIVING;
        
        send_ack();
        SEGGER_RTT_printf(0, "[OTA] Erase done, ready for data.\r\n");
        break;
    }
    
    case CMD_DATA:
    {
        if (ota_state != OTA_RECEIVING) return;
        
        // payload: [2B block_num][data...]
        uint16_t blk = *(uint16_t *)(&frame->data[0]);
        uint8_t *payload = &frame->data[2];
        uint16_t payload_len = frame->data_len - 2;
        
        if (Flash_Write(ota_write_addr, payload, payload_len) != 0)
        {
            send_nak(ERR_FLASH_WRITE);
            ota_state = OTA_ERROR;
            return;
        }
        
        ota_write_addr += payload_len;
        ota_block_count++;
        
        // 每 50 块打印一次进度
        if (ota_block_count % 50 == 0)
        {
            uint32_t progress = (ota_write_addr - APP_BASE) * 100 / ota_total_size;
            SEGGER_RTT_printf(0, "[OTA] Progress: %d%%\r\n", progress);
        }
        
        send_ack();
        break;
    }
    
    case CMD_END:
    {
        // payload: [2B total_blocks]
        uint16_t expected_blks = *(uint16_t *)(&frame->data[0]);
        
        SEGGER_RTT_printf(0, "[OTA] END: received %d blocks (expected %d)\r\n",
                          ota_block_count, expected_blks);
        SEGGER_RTT_printf(0, "[OTA] Verifying CRC32...\r\n");
        
        // TODO: 计算整个 App 区的 CRC32，与 ota_crc32_expected 对比
        // uint32_t actual_crc = crc32_calc(APP_BASE, ota_total_size);
        // if (actual_crc != ota_crc32_expected) { ... }
        
        send_ack();
        ota_state = OTA_COMPLETE;
        SEGGER_RTT_printf(0, "[OTA] SUCCESS! Rebooting to new App...\r\n");
        break;
    }
    
    case CMD_VERSION:
        send_version();
        break;
    }
}

// 发送 ACK: AA 80 00 00 80 55
void send_ack(void)
{
    uint8_t ack[] = {0xAA, RSP_ACK, 0x00, 0x00, RSP_ACK, 0x55};
    HAL_UART_Transmit(&huart2, ack, 6, 100);
}

// 发送 NAK
void send_nak(uint8_t error_code)
{
    uint8_t nak[] = {0xAA, RSP_NAK, 0x01, 0x00, error_code, error_code ^ RSP_NAK, 0x55};
    HAL_UART_Transmit(&huart2, nak, 7, 100);
}

// 发送版本信息
void send_version(void)
{
    uint8_t data[8];
    data[0] = (BOOTLOADER_VERSION >> 8) & 0xFF;   // 版本高字节
    data[1] = BOOTLOADER_VERSION & 0xFF;           // 版本低字节
    // App 大小...
    uint8_t x = RSP_VERSION;
    for (int i = 0; i < 6; i++) x ^= data[i];      // XOR8 校验
    
    HAL_UART_Transmit(&huart2, (uint8_t[]){0xAA, RSP_VERSION, 6, 0, data[0], data[1], 0, 0, 0, 0, x, 0x55}, 12, 100);
    // ↑ 简化写法，建议分步构造
}
```

#### 8.6.3 验证流程

**没有手机端 App 的情况下**，用蓝牙调试助手手动模拟：

1. **查询版本**：发送 `AA 10 00 00 10 55`
   - 期望：收到版本响应 `AA 82 ...`

2. **开始升级**：发送 `AA 01 0A 00 [10字节payload] [XOR8] 55`
   - Payload: 4B 文件大小 + 4B CRC32 + 2B 版本
   - 期望：收到 `AA 80 00 00 80 55`（ACK）

3. **发送数据块**：发送 `AA 02 [LEN] [2B块号][数据] [XOR8] 55`
   - 期望：每帧收到 ACK

4. **结束升级**：发送 `AA 03 02 00 [2B总块数] [XOR8] 55`
   - 期望：收到 ACK + RTT 打印 "OTA SUCCESS"

#### 8.6.4 检查点

| # | 检查项 | 完成 |
|---|--------|------|
| 1 | 版本查询 → 收到正确版本响应 | ⬜ |
| 2 | CMD_START → 擦除成功 → 收到 ACK | ⬜ |
| 3 | 连续发送 10 帧 CMD_DATA → 全部 ACK，无丢帧 | ⬜ |
| 4 | CMD_END → ACK → RTT 打印 "OTA SUCCESS" | ⬜ |

---

### 8.7 新手常见误区

| 误区 | 真相 |
|------|------|
| "环形缓冲要在中断里做复杂处理" | 中断里只做 `Put` 一个动作，耗时微秒级 |
| "Flash 写入之前不需要擦除" | **必须先擦后写**，Flash 只能从 1→0 |
| "擦 App 区的时候把 Bootloader 也擦了" | Sector 号对就绝对不会——Sector 0-1 是 Bootloader，Sector 3-7 是 App |
| "帧解析应该等收完一帧再处理" | 状态机就是逐字节喂的，不需要"等收完" |
| "手机发多快 STM32 都能跟上" | HC-08 波特率 9600 = 每秒 ~960 字节，Flash 写入速度远大于这个，理论上没问题 |

---

## 9. 第 4.5 课：Keil 硬件调试实战 🔥

**日期**: 2026-07-22

> 本节是第 3 课完成后的实战调试记录——Bootloader 跳转后 App 不运行。通过这次调试，实习生掌握了二分定位法、RTT 探针法、Keil 硬件断点调试、反汇编分析和半主机问题排查。这是整个培训中最有价值的一课。

### 9.1 问题现象

**环境**：F407_IAR Bootloader (0x08000000) + PanelPro App (0x0800C000)，一块板子。

**现象**：Bootloader 正常打印 "APP valid, jump to app"，但跳转后 App 无任何 RTT 输出，LVGL 不显示。Keil 调试模式下全速运行后暂停，CPU 停在：

```
0x0801D64A  BEAB   BKPT  0xAB
PC = 0x0801D64A
```

### 9.2 调试方法论：二分定位法

整个启动流程是一根链条：

```
上电 → Bootloader → 跳转 → Reset_Handler → SystemInit() → __main() → main() → FreeRTOS → LVGL
```

**核心思路**：在链条上不断插"探针"（RTT 打印），每次把范围缩小一半。就像在一根绳子找断点——先看中间是否断了，再看四分之一处。

### 9.3 第一层：探针法 — RTT 分层追踪

**探针 1**：Bootloader 跳转前打印 App 向量表

```
[DIAG] Vector: 48 CA 01 20 61 C2 00 08
[DIAG] Code@PC: 48 80 47 06 48 00 47 FE E7 FE ...
```

- MSP = 0x2001CA48（合法 SRAM 地址）✅
- PC = 0x0800C261（在 App Flash 范围，Thumb 模式）✅
- 向量表内容不是 0xFF（Flash 确实烧进去了）✅
- 反汇编确认是标准 Keil 启动序列：`LDR R0,=SystemInit → BLX R0 → LDR R0,=__main → BX R0` ✅

**判断**：Bootloader 跳转正常，App 启动代码正常。崩溃在 SystemInit() 或 __main() 内部。

**探针 2**：在 App 的 main() 最开头加 RTT 打印

```c
int main(void) {
    SEGGER_RTT_printf(0, "[APP] Entered main()\r\n");  // ← 探针
    HAL_Init();
    ...
}
```

**结果**：看到了 `[APP] Entered main()` → 说明 **main() 跑到了**，之前判断"崩溃在启动代码"被推翻。SystemInit 中的 RTT 打印没出来是因为 SEGGER RTT 的控制块存在 `.bss` 段，在 `__main()` 清零 `.bss` 之前是垃圾数据，RTT 无法工作。

**探针 3**：在 main() 各初始化函数之间加探针

```c
HAL_Init();
SEGGER_RTT_printf(0, "[APP] HAL_Init done\r\n");
SystemClock_Config();
SEGGER_RTT_printf(0, "[APP] Clock done\r\n");
...
osKernelInitialize();
SEGGER_RTT_printf(0, "[APP] Kernel init done\r\n");
MX_FREERTOS_Init();
SEGGER_RTT_printf(0, "[APP] Task create done\r\n");
osKernelStart();
SEGGER_RTT_printf(0, "[APP] Scheduler started\r\n");
```

**结果**：最后看到 `[APP] Task create done`，未看到 `[APP] Scheduler started`。

**定位**：崩溃在 `osKernelStart()` 内部。范围从"整个 main()"缩小到"一个函数调用"。

### 9.4 第二层：Keil 硬件调试 — 读 PC + 反汇编

当探针法精确到一个函数后，切换到 Keil 硬件调试：

**操作**：
1. Ctrl+F5 进入 Keil Debug 模式
2. F5 全速运行几秒（让 App 初始化然后卡死）
3. 点击 Stop 暂停
4. 查看 **Disassembly 窗口**（View → Disassembly Window）和 **Register 窗口**（PC、xPSR）

**观察到**：
```
PC   = 0x0801D64A
xPSR = 0x21000000  → 异常号 = 0x00（不在异常里，CPU 正常执行状态）

0x0801D642  STRB  r0, [sp, #0x00]   // 存字符到栈上
0x0801D646  MOV   r1, sp            // r1 = 栈地址
0x0801D648  MOVS  r0, #0x03         // r0 = 3 = SYS_WRITEC
0x0801D64A  BKPT  0xAB              // ← 停在这里
```

**关键知识点**：`MOVS r0, #0x03 + BKPT 0xAB` 是 ARM 半主机（Semihosting）协议的标准调用序列。`r0=3` 表示 `SYS_WRITEC`——"向调试器控制台写一个字符"。CPU 执行 `BKPT 0xAB` 后暂停，等待调试器处理。没有调试器时，BKPT 指令会导致 CPU 停住或进 HardFault。

### 9.5 第三层：Map 文件溯源

在 PanelPro 的 `pro.map` 中搜索 `0x0801D63F`（附近的函数起始地址）：

```
fputc    0x0801d63f  Thumb Code  18  fputc.o(i.fputc)
```

18 字节刚好是那段汇编的长度。进一步搜索：

```
fputc.o(i.fputc) refers to __I$use$semihosting$fputc
```

**根因确认**：MicroLIB 的 `fputc` 函数使用了半主机来实现字符输出。PanelPro 工程中没有提供自定义的 `fputc`（usart.c 不存在），所以链接了 MicroLIB 的版本。

另外一条线索来自 map：
```
../clib/microlib/stdio/fputc.c   0x00000000   Number  0  fputc.o ABSOLUTE
```
确认 fputc 来自 MicroLIB 库。

**调用链**：`printf()` → MicroLIB `fputc()` → 半主机 `BKPT 0xAB` → CPU 暂停

### 9.6 根因与修复

**根因**：PanelPro 工程没有提供自定义的 `fputc` 函数，MicroLIB 的半主机版 `fputc` 在脱机运行时 `BKPT 0xAB` 无法被处理，导致 CPU 停止。

**修复**：在 `Core/Src/main.c` 中提供 RTT 版 `fputc`，覆盖 MicroLIB 的实现：

```c
/* USER CODE BEGIN 4 */
#include <stdio.h>

int fputc(int ch, FILE *f)
{
    SEGGER_RTT_PutChar(0, ch);
    return ch;
}
/* USER CODE END 4 */
```

项目已开启 MicroLIB（`Use MicroLIB` 已勾选），用户代码中的 `fputc` 会优先于库中的版本被链接。

### 9.7 调试推理链总结

```
PC = 0x0801D64A                        → CPU 在 App 代码区，没飞
  ↓
指令 = BKPT 0xAB                       → ARM 半主机断点
  ↓
上下文: r0=3, r1=sp                    → SYS_WRITEC：写字符到调试器
  ↓
map: 0x0801D63F ∈ fputc.o             → 是 C 库的 fputc 函数
  ↓
map: fputc → __I$use$semihosting       → MicroLIB 用了半主机
  ↓
PanelPro 无 usart.c / 自定义 fputc     → 无用户覆盖 → printf 走半主机
  ↓
脱机无调试器 → BKPT 无人处理 → 死      → 根因确认
```

**五层推理，每层都基于数据（PC值、反汇编、寄存器、map文件），没有靠猜。**

### 9.8 本课学到的调试技能

| 技能 | 工具 | 适用场景 |
|------|------|---------|
| RTT 探针法 | SEGGER_RTT_printf | 快速缩小崩溃范围，不需要调试器 |
| 向量表验证 | Bootloader 读 Flash 原始字节 | 确认 App 是否正确烧录 |
| PC + 反汇编分析 | Keil Debug + Disassembly | 精确到指令级定位 |
| xPSR 寄存器解读 | Keil Register 窗口 | 判断 CPU 是否在异常中 |
| Map 文件溯源 | .map 文本搜索 | 地址→函数→库，完整调用链 |
| 手工 Thumb 反汇编 | 对比 ARM 指令编码表 | 理解启动代码和半主机协议 |

---

## 10. 培训日志

| 日期 | 内容 | 结果 |
|------|------|------|
| 2026-07-17 | 第 1 课 Echo 测试 | ✅ 通过 — 手机发送字符，STM32 原样返回 |
| 2026-07-17 | 第 2 课 任务 2-3：boot_config.h + boot_jump.c | ✅ 通过 — 分区地址正确，跳转逻辑正确 |
| 2026-07-18 | 第 2 课 任务 4：boot_main.c | ⚠️ Code Review 发现 3 个 Bug，需修复 |
| 2026-07-19 | 导师 Code Review — 3 个 Bug 修复 | ✅ 全部修完 |
| 2026-07-19 | **任务 5**：创建 Bootloader Keil 工程 — 编译 0 错误 0 警告 | ✅ 通过 |
| 2026-07-19 | **任务 6**：烧录 Bootloader → RTT 验证 — 输出符合预期 | ✅ 通过 |
| 2026-07-19 | **第 3 课**：App 工程重定位 — LVGL 正常显示，双工程架构跑通 | ✅ 通过 |
| 2026-07-19 | **第 4 课**：Flash 操作 + 协议解析 | 🔥 进行中 |
| 2026-07-22 | **第 4.5 课**：Keil 硬件调试实战 — Bootloader→App 崩溃排查全程 | ✅ 通过 — 掌握二分定位法、反汇编分析、Map文件溯源 |
| 2026-07-22 | **Bug 修复**：PanelPro 添加 RTT 版 fputc，解决半主机 BKPT 0xAB 崩溃 | ✅ 已修复 |
| 2026-08-07 | **第 4 课 任务 2 验收**：帧解析状态机 — 手机发 `AA 10 00 00 00 55` → RTT 正确识别 `CMD:0x10 LEN:0` | ✅ 通过 |
| 2026-08-07 | **第 4 课 任务 3**：Flash 操作 — 擦除 Sector 3-7 ✅ + WORD 编程 256 字节回读一致 ✅ + 自测 PASS ✅ | ✅ 通过 |
| 2026-08-07 | **代码清理**：flash_ops 命名统一、返回语义统一（0=成功）、条件打印替代误导性输出 | ✅ 完成 |
| 2026-08-07 | **经验教训**：HAL 输出参数（SectorError）传 NULL 是未定义行为 → 必须传真变量 | ✅ 已记录 |
| 2026-08-12 | **第 4 课 任务 4a**：Metadata 模块 — Sector 2 写读 magic/size/version，IsValid 验证 PASS | ✅ 通过 |
| 2026-09-01 | **OTA Manager 模块**：响应层两层（OTA_Send_Response_Data/OTA_Send_Response）+ OTA_HandleFrame 分发器 + CMD_VERSION（读 Metadata 回版本 / 无效回 NACK），上板验证 `AA 10 00 00 00 55` → `AA 82 04 00 01 01 00 00 00 55` | ✅ 通过 |

---

### 📋 当前待办清单（2026-08-07）

按优先级排列：

| # | 任务 | 预计耗时 | 完成标准 |
|---|------|---------|---------|
| 1 | ✅ 第 4 课 任务 1：环形缓冲区 | — | 已完成 |
| 2 | ✅ 第 4 课 任务 2：帧解析状态机 | — | 已完成 |
| 3 | ✅ 第 4 课 任务 3：Flash 操作 | — | 已完成 |
| 4 | 🔥 **第 4 课 任务 4：OTA 完整流程串联**（4a Metadata✅ / OTA Manager+CMD_VERSION✅ / 4b CMD_START 下一步） | 90 分钟 | CMD_START 擦除 → CMD_DATA 写入 → CMD_END 校验 → 全部 ACK |
| 5 | 第 5 课：App 端 OTA 触发（LVGL 按钮） | 60 分钟 | 按钮触发升级流程 |
| 6 | 第 6 课：异常处理 + 恢复机制 | 60 分钟 | 超时/CRC/强制恢复 |

---

> **导师备注**：本文档随培训进度持续更新。实习生完成每个任务后将代码提交导师 review，review 通过后更新本文档的检查点和日志。每个 Bug 修复单独 commit，养成原子提交的习惯。
