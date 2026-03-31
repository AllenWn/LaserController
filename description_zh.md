# NIR 激光控制器固件系统设计说明（当前设计基线）

## 1. 文档定位

本文档是当前项目的**系统逻辑设计基线**。  
它的目标不是复述现有代码，而是完整定义：

- 固件必须实现的系统行为
- 安全闭环的输入、判定、输出和恢复逻辑
- 各板级模块在系统中的角色
- 各通信器件和传感器应如何接入
- 哪些内容已经锁定，哪些内容仍是待确认项

该文档应足够详细，使另一个实现者或另一个 LLM 在不依赖当前代码细节的情况下，也能设计并实现一套**行为一致、语义一致、安全策略一致**的固件。

如果后续代码与本文档冲突，应以本文档为准，代码应被修正。

## 2. 系统目标

本系统是一个基于 `ESP32-S3-WROOM-1U` 的手持式近红外激光设备控制器。  
MCU 的职责是**监督控制**，而不是底层模拟闭环控制。

MCU **不直接实现**：

- 激光电流闭环
- TEC 温控闭环
- USB-C PD 协议协商

这些功能由独立硬件器件完成。

MCU **必须实现**：

- 系统上电顺序与电源域管理
- 子系统健康监测
- 操作者输入处理
- 姿态安全判断
- 发射许可判断
- 状态机控制
- 故障分类、锁存、恢复
- 对 LD / TEC / DAC / 辅助外设的上层协调

因此，本固件应被视为一个**实时监督安全控制器**。

## 3. 顶层控制原则

固件围绕两个不同但相关的问题运行：

### 3.1 问题一：系统是否允许保持上电

这是 **power supervision** 问题。  
它决定：

- TEC 电源域是否允许打开
- LD 电源域是否允许打开
- 出现电源类故障时是否必须掉电

### 3.2 问题二：当前是否允许激光发射

这是 **emission permit** 问题。  
它决定：

- 激光是否允许从待命态进入真正发射
- `LD_SBDN` 是否可以进入 `operation`

### 3.3 两者必须分离

系统可能处于：

- 已上电但不允许发射
- 已上电且允许待命
- 已上电且允许发射
- 发生电源/控制链故障，必须关断

因此固件必须分别管理：

- `power enable`
- `permit`
- `trigger`
- `LD operating state`
- `DAC active/safe output`

## 4. 安全哲学

### 4.1 Fail-safe

只要存在不确定性，系统必须趋向更安全的输出。

例如：

- 必要监测数据过期 -> 不允许发射
- 关键通信失败 -> 不允许发射，必要时故障锁存
- 电源健康丢失 -> 必要时直接掉电
- trigger 状态不可信 -> 不允许发射

### 4.2 Supervisor-over-hardware

固件不替代底层硬件保护，而是对硬件状态进行监督、分类和升级响应。

### 4.3 No direct trigger-to-laser path

按钮输入不得直接驱动激光。  
按钮只能作为 supervisor 的一个输入条件。

### 4.4 Single point of truth

系统状态转移、permit 判定、故障分类、输出目标值，必须由 supervisor + state machine 统一决定。  
其他任务不得私自改变核心安全输出。

### 4.5 Fresh-data only

所有关键决策都必须基于“新鲜、有效、已验证”的系统快照。

## 5. 硬件系统组成

完整系统目前至少由以下板级部分组成：

### 5.1 MainBoard

主板承载：

- ESP32-S3 主控
- LD driver 相关接口
- TEC controller 相关接口
- DAC80502
- LSM6DSO IMU
- 部分电源控制与健康输入
- 主安全闭环的主要直接接口

### 5.2 Auxiliary Board

辅助板承载：

- `MCP23017` GPIO expander
- `TLC59116` LED driver
- `DRV2605` 振动驱动
- 操作按钮
- 板上共享 I2C 总线

该板主要提供**人机交互与辅助输出功能**。

### 5.3 PD Board

PD 板承载：

- `STUSB4500`
- USB-C / PD 前端相关电路

该板负责 PD 协商与相关电源前端行为。  
MCU 不直接实现 PD 协议，但可以通过 I2C 对 `STUSB4500` 做监督与配置。

### 5.4 BMS / Power Path Board

该板主要承载：

- `TPS2121`
- 电源路径选择与跨板连接转接

该板目前不是 MCU 直接驱动对象，更多是电源拓扑的一部分。

## 6. 固件功能分层

### 6.1 硬件接口层

负责：

- GPIO
- SPI
- I2C
- 片上 ADC
- 日志与调试接口

### 6.2 驱动层

负责各外设或板载器件的最小通信能力，例如：

- `LSM6DSO`
- `DAC80502`
- `MCP23017`
- `TLC59116`
- `DRV2605`
- `STUSB4500`

### 6.3 监测层

负责把原始接口数据转换成模块级状态，例如：

- 电源健康
- LD 数字状态
- TEC 数字状态
- IMU 姿态安全
- 模拟量遥测
- trigger 状态
- PD 状态

### 6.4 系统快照层

负责存储统一的系统状态快照，并带时间戳。

### 6.5 Supervisor 层

负责：

- freshness 检查
- permit 计算
- 故障分类
- 锁存和清除
- 给状态机提供输入
- 生成 DAC clamp / setpoint 策略

### 6.6 状态机与输出层

负责把 supervisor 结果落实为：

- 电源使能
- `LD_SBDN` 三态
- DAC safe / ready / firing 输出
- 其他固定控制输出

### 6.7 UI / Auxiliary 外围层

负责：

- LED 状态显示
- 振动反馈
- 按钮输入采集
- 非安全关键的用户交互

### 6.8 PD 监督层

负责：

- `STUSB4500` 的 I2C 可达性
- PD attach / contract / alert/fault 的读取与解释
- 向系统快照提供 PD 状态

当前阶段，该层**不直接进入主安全 permit 闭环**。

## 7. 主安全闭环输入

### 7.1 Power 输入

- `PWR_TEC_GOOD`
- `PWR_LD_PGOOD`

作用：

- 电源域健康确认
- 上电顺序确认
- 运行中 power-drop 检测

### 7.2 LD 数字健康输入

- `LD_LPGD`

语义：

- `HIGH = loop good`
- `LOW = loop not good`

### 7.3 TEC 数字健康输入

- `TEC_TEMPGD`

语义：

- `HIGH = thermal condition acceptable`
- `LOW = thermal condition not acceptable`

### 7.4 IMU 姿态输入

通过 SPI 从 IMU 获取姿态相关数据，派生出：

- `ORIENTATION_OK`

当前设计目标：

- 设备应主要向下工作
- 当激光方向抬升超过允许阈值时，不允许发射

### 7.5 Trigger 输入

当前系统设计中，最终 trigger 来自 `Auxiliary` 板按钮链路：

- `SW1`
- `MCP23017 GPA0`
- `MCP23017 GPA1`
- MCU 通过 I2C 读取 expander 寄存器得到按钮状态

当前规则：

- `GPA0 = low` 且 `GPA1 = low` 时才视为 trigger 按下
- 不接受“任意一路低就算触发”的策略

### 7.6 模拟量输入

当前已识别的模拟量输入包括：

- `LD_TMO`
- `LD_LIO`
- `TEC_TMO`
- `TEC_ITEC`
- `TEC_VTEC`

这些信号通过 ESP32 片上 ADC 读取。  
当前项目中，ADC 基础接入已确定：

- `GPIO1 -> ADC1_CH0`
- `GPIO2 -> ADC1_CH1`
- `GPIO8 -> ADC1_CH7`
- `GPIO9 -> ADC1_CH8`
- `GPIO10 -> ADC1_CH9`

默认使用：

- `ADC_UNIT_1`
- `ADC_ATTEN_DB_12`

这些模拟量当前主要用于遥测与扩展安全逻辑准备，是否进入 permit/fault 仍属于策略待定项。

## 8. 主安全闭环输出

### 8.1 电源输出

- `PWR_TEC_EN`
- `PWR_LD_EN`

### 8.2 激光模式控制输出

- `LD_SBDN`

其语义为：

- `LOW` -> `Shutdown`
- `High-Z` -> `Standby`
- `HIGH` -> `Operation`

### 8.3 DAC 输出

DAC 输出不是安全判定本身，而是安全状态下的从属控制输出。

- `DAC channel A` -> LD analog command path
- `DAC channel B` -> TEC analog command path

当前系统中 DAC 的关键输出状态为：

- `LD safe code`
- `TEC safe code`
- `TEC ready code`
- `LD firing code`

这些值的直接单位是 **DAC code**，其工程意义分别对应：

- LD 目标电流设定
- TEC 目标温度设定或相关模拟设定

## 9. MainBoard 直接控制逻辑

### 9.1 上电顺序

状态机应至少包含以下上电流程：

1. `OFF`
2. `POWERUP_TEC`
3. `POWERUP_LD`
4. `READY`
5. `FIRING`

推荐顺序：

1. 初始安全状态：全部关断
2. 打开 `TEC` 电源域
3. 等待 `TEC PGOOD`
4. 打开 `LD` 电源域
5. 等待 `LD PGOOD`
6. 进入 `READY`

若任一步超时，则进入 internal fault。

### 9.2 READY 语义

在 `READY` 状态下：

- 两路电源域可保持使能
- DAC 输出处于 ready/standby 所需目标
- `LD_SBDN = High-Z`，即 `Standby`
- 激光不得真正发射

### 9.3 FIRING 语义

进入 `FIRING` 的条件至少包括：

- 已处于 `READY`
- `permit = true`
- `trigger = true`

在 `FIRING` 状态下：

- `LD_SBDN = HIGH`
- DAC 允许进入 firing setpoint

## 10. 故障分类

系统故障应至少分成两类：

### 10.1 Internal Fault

表示内部电气/控制链路问题，例如：

- PGOOD 丢失
- 上电超时
- LD / TEC 关键数字健康失效
- 关键控制链路异常

对该类故障的原则：

- 需要锁存
- 通常掉 EN
- 需要重新走上电流程恢复

### 10.2 Usage Fault

表示使用过程超出允许包络，例如：

- IMU 姿态越界
- 后续可能加入 distance 越界

对该类故障的原则：

- 禁止发射
- 不一定掉主电源域
- 可在恢复后回到 `READY`

## 11. 状态机定义

状态机当前设计应至少包含：

- `OFF`
- `POWERUP_TEC`
- `POWERUP_LD`
- `READY`
- `FIRING`
- `FAULT_INTERNAL`
- `FAULT_USAGE`

### 11.1 OFF

- 两路 EN 低
- `LD_SBDN = shutdown`
- DAC = safe

### 11.2 POWERUP_TEC

- `TEC_EN = 1`
- `LD_EN = 0`
- 激光不允许发射

### 11.3 POWERUP_LD

- `TEC_EN = 1`
- `LD_EN = 1`
- 等待 LD power good

### 11.4 READY

- 两路 EN 保持
- `LD_SBDN = standby`
- DAC 输出 ready / standby 所需值

### 11.5 FIRING

- 两路 EN 保持
- `LD_SBDN = operation`
- DAC 输出 firing 目标值

### 11.6 FAULT_INTERNAL

- 至少强制 `LD_SBDN = shutdown`
- 通常拉低相关 EN
- DAC 回 safe

### 11.7 FAULT_USAGE

- 强制 `LD_SBDN = shutdown`
- EN 可保持
- DAC 回 safe 或非发射状态值

## 12. Permit 逻辑

当前设计中，`permit` 至少依赖以下条件：

- `TEC power ready`
- `LD power ready`
- `TEC digital good`
- `LD digital good`
- `IMU orientation OK`
- 无 internal fault latch
- 无 usage fault latch

注意：

- `trigger` **不属于 permit 本身**
- `trigger` 是从 `READY -> FIRING` 的附加条件

即：

- `permit` 决定“系统是否允许发射”
- `trigger` 决定“操作者此刻是否请求发射”

## 13. Trigger 逻辑

Trigger 链路必须满足：

- 物理按钮输入来自 Auxiliary 板
- 经 `MCP23017` 扩展到 I2C
- MCU 读取 expander 状态
- 经过去抖后再形成 `trigger_pressed`

按钮判定规则：

- `GPA0 == low && GPA1 == low` -> `pressed`
- `GPA0 == high && GPA1 == high` -> `released`
- `GPA0 != GPA1` -> 视为不一致状态，不得算作按下

## 14. Auxiliary 板设计语义

### 14.1 MCP23017

作用：

- 采集按钮输入
- 提供辅助 GPIO
- 控制 `LRA_EN`

它不是按钮本体，而是按钮与主控之间的 I2C GPIO expander。

### 14.2 TLC59116

作用：

- 状态灯 / 指示灯驱动
- 非安全关键输出

### 14.3 DRV2605

作用：

- 振动马达 / LRA / ERM 驱动
- 用于人机反馈
- 非安全关键输出

### 14.4 Auxiliary 与主闭环的关系

只有**按钮输入**属于主控制链的一部分。  
LED 与振动都不属于主安全裁决闭环。

## 15. PD 板设计语义

### 15.1 STUSB4500 的定位

`STUSB4500` 是独立 PD sink controller。  
MCU 不负责实现 PD 协议栈，只通过 I2C 对其做监督/配置。

### 15.2 当前固件中 PD 的角色

当前阶段：

- PD 属于独立监督任务
- 不直接进入主安全 permit 闭环

当前已实现的 PD 语义包括：

- 芯片是否可通过 I2C 访问
- attach / contract / alert/fault 的状态框架

但寄存器地址和 bit 含义中仍有待最终核实项。

## 16. 模拟量设计语义

### 16.1 LD 模拟量

已知：

- `LD_LPGD`：`HIGH = GOOD`
- `LD_TMO` 近似公式：
  - `T(°C) ≈ 192.5576 - 90.1040 × V_TMO`
- `LD_LIO` 近似公式：
  - `I_OUT = V_LIO × 2.4`

待定：

- 板级缩放
- 安全阈值
- 是否纳入 permit/fault

### 16.2 TEC 模拟量

已知：

- `TEC_TEMPGD`：`HIGH = GOOD`
- `ITEC = (V_ITEC - 1.25) / 0.285`

待定：

- `TMO -> °C` 最终映射
- `VTEC` scaling
- 安全阈值
- 是否纳入 permit/fault

## 17. 实时任务设计

当前推荐的软件结构是：

- `digital monitor task`
- `analog monitor task`
- `imu task`
- `pd task`
- `aux button task`
- `aux output task`
- `dac task`
- `supervisor task`

基本原则：

- monitor 任务负责采样和更新快照
- supervisor 负责统一裁决
- state machine 负责全局状态
- 输出任务负责外围执行

## 18. 数据所有权原则

### 18.1 Monitor 任务

只负责更新自己的快照，不直接改状态机。

### 18.2 Supervisor

唯一负责：

- permit
- fault classification
- clear policy
- state machine inputs

### 18.3 State Machine

唯一负责：

- 状态转移
- 输出目标定义

### 18.4 外围输出任务

只消费系统状态，不得反过来决定核心安全行为。

## 19. 当前已锁定内容

以下内容当前已经基本锁定：

- `LD_SBDN` 三态语义
- `READY -> standby`
- `READY + permit + trigger -> FIRING`
- `internal fault` 与 `usage fault` 分级
- MainBoard 主要 direct IO mapping
- `DAC80502` I2C 地址 `0x48`
- Auxiliary:
  - `TLC59116` 地址 `0x60`
  - `DRV2605` 地址 `0x5A`
- ADC 基础 GPIO -> channel 映射
- PD 物理 I2C 路径（`GPIO4/5`）

## 20. 当前仍待补全的开放项

以下内容仍未最终锁定，后续需要继续补充：

### 20.1 Auxiliary host pin mapping

仍需确定：

- `AUX_I2C_SCL`
- `AUX_I2C_SDA`
- `GPIO_INT_A`

最终落到 ESP32 哪些 GPIO。

### 20.2 IMU 激光轴定义

需确定：

- 激光出射方向在 IMU 坐标系中对应哪根轴

### 20.3 DAC 最终业务设定值

仍需定义：

- LD safe code
- TEC safe code
- TEC ready code
- LD firing code

### 20.4 STUSB4500 寄存器语义

仍需锁定：

- attach 对应寄存器和 bit
- contract_valid 对应寄存器和 bit
- alert/fault 对应寄存器和 bit

### 20.5 模拟量策略

仍需锁定：

- 模拟量是否仅用于遥测
- 还是进入 permit
- 或进入 fault

### 20.6 Distance sensor

仍需锁定：

- 实际器件
- 接口形式
- 是否作为 usage fault 输入

## 21. 实现要求

任一后续实现必须满足以下原则：

- 逻辑上与本文档一致
- 安全行为不弱于本文档定义
- 允许采用更优的软件结构
- 允许采用与当前代码不同的实现细节
- 但不得改变状态语义、fault 语义、permit 语义和关键输出语义

## 22. 与设计资料的关系

本文档必须与以下资料结合使用：

- `design_src/firmware_interface_lock_v2.*`
- `design_src/cross_board_connector_map.json`
- `design_src/*/semantic/*.json`
- `design_src/datasheet_notes/*.md`

这些资料提供：

- 板级连接关系
- 器件语义
- 引脚和总线信息
- 当前仍未闭合的设计空缺

本文档负责定义：

- 系统行为
- 逻辑层约束
- 安全与控制策略

