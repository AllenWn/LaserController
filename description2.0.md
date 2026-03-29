# NIR 激光控制器固件系统设计规范（修订版 / 可实现版）

## 1. 文档目的

本文档定义基于 ESP32-S3-WROOM-1U 的手持式近红外外科激光控制器固件系统设计规范。  
本文档是该固件的最高层逻辑参考，目标是：

- 定义系统行为
- 定义安全模型
- 定义状态机与输出策略
- 定义监测、快照、监督、故障与恢复逻辑
- 定义硬件接口在固件中的抽象语义
- 定义实时软件架构与职责边界

本文档应足够详细，使实现者能够仅依据本文档构建出功能一致、行为一致、安全策略一致的完整固件。

若代码实现与本文档冲突，应以本文档为准，并修改代码以符合本文档。

## 2. 系统定义

本系统中的 MCU 固件是一个实时监督安全控制器。  
它不直接实现以下底层闭环控制：

- LD 激光电流闭环
- TEC 温度闭环
- USB-C PD 协议协商

这些功能由独立专用硬件完成。

MCU 固件必须实现以下职责：

- 系统启动与上电序列监督
- 电源域控制与电源健康确认
- 子系统数字健康状态采集
- 姿态安全判断
- 操作者请求输入处理
- 统一安全裁决
- 系统状态机控制
- 故障分类、锁存、清除与恢复
- LD 模式控制
- DAC 安全钳位与激活策略
- 调试与日志输出

## 3. 顶层控制原则

系统围绕两个彼此独立但相互关联的问题运行：

### 3.1 问题一：系统是否允许保持上电运行

这是 power-domain supervision 问题。

### 3.2 问题二：当前是否允许激光发射

这是 emission permit 问题。

两者必须严格分离。

系统可能处于以下情况：

- 已上电但不允许发射
- 已上电且允许待命
- 已上电且允许发射
- 不可信，必须断电

因此，固件必须分别管理：

- power enable
- emission permit
- LD operating mode
- DAC active/safe mode

## 4. 安全哲学

### 4.1 Fail-safe

任何不确定性都必须收敛到更安全的输出行为。

### 4.2 Supervisor-over-hardware

固件不替代底层硬件保护，而是对其进行监督、分类和升级响应。

### 4.3 No direct trigger-to-laser path

操作员 trigger 不得直接驱动激光。  
trigger 只能作为 supervisor 的一个输入条件。

### 4.4 Single point of truth

系统状态转移、permit 判定和安全输出目标值，必须由 supervisor/state-machine 统一决定。  
其他任务不得私自更改核心安全输出。

### 4.5 Fresh-data only

任何关键判定仅可基于“新鲜且已验证”的监测数据作出。

## 5. 固件功能分层

### 5.1 Hardware Abstraction Layer

负责 GPIO、SPI、I2C、ADC、定时器、日志接口等底层访问。

### 5.2 Monitor Layer

负责采样原始信号并生成子系统级监测结果。

包括：

- power monitor
- ld monitor
- tec monitor
- imu monitor
- trigger monitor
- optional analog monitor

### 5.3 System Snapshot Layer

负责维护统一、带时间戳的系统快照。

### 5.4 Supervisor Layer

负责：

- freshness 判断
- permit 计算
- fault 归类
- fault latch / clear
- state transition input 生成
- 输出目标生成

### 5.5 State / Output Layer

负责把 supervisor 结果落实为稳定硬件行为：

- 电源域开关
- SBDN 三态
- DAC safe/ready/fire 目标
- 固定配置输出

## 6. 版本边界

### 6.1 本版本必须实现

- 电源域上电顺序
- PGOOD 监督
- TEC_TEMPGD 数字健康检查
- LD_LPGD 数字健康检查
- IMU 姿态安全判定
- trigger 输入处理
- internal / usage fault 分类
- LD_SBDN 三态控制
- DAC 安全钳位与 firing 激活策略

### 6.2 本版本已知存在但不纳入核心闭环

- LD_TMO
- LD_LIO
- TEC_TMO
- TEC_ITEC
- TEC_VTEC
- IMU_INT2
- ERM_*
- USB 非安全通信
- distance sensor

### 6.3 后续版本可扩展

- 模拟阈值 fault
- distance usage fault
- 中断辅助快速关断
- 分 fault-source 的差异化 clear 策略
- service-only clear 策略

## 7. 接口语义规范

### 7.1 系统 / 启动 / 调试接口

- EN：MCU 复位/使能输入。固件不得将其作为普通运行期 IO 使用。
- GPIO0：boot strap，保留。
- GPIO46：boot strap，保留。
- UART0 TX/RX：用于日志、调试、下载。
- USB D+/D-：用于 USB 通信/下载/调试，不纳入安全闭环。

### 7.2 电源域接口

- PWR_TEC_EN：输出，高有效，控制 TEC 电源域。
- PWR_TEC_GOOD：输入，高有效，表示 TEC 电源域正常。
- PWR_LD_EN：输出，高有效，控制 LD 电源域。
- PWR_LD_PGOOD：输入，高有效，表示 LD 电源域正常。

### 7.3 LD 接口

- LD_SBDN：三态控制接口。
  输出低：shutdown
  高阻输入态：standby
  输出高：operate
- LD_LPGD：输入，高有效，LD 环路正常。
- LD_PCN：输出，当前版本固定为高支路选择。
- LD_TMO / LD_LIO：模拟监测输入，当前版本不参与核心闭环。

### 7.4 TEC 接口

- TEC_TEMPGD：输入，高有效，热状态正常。
- TEC_TMO / TEC_ITEC / TEC_VTEC：模拟监测输入，当前版本不参与核心闭环。

### 7.5 IMU 接口

IMU 通过 SPI 接入。  
固件必须初始化 IMU 并持续获取姿态相关数据。  
固件必须输出派生安全结果 ORIENTATION_OK。  
IMU_INT2 当前版本不作为闭环必需条件。

### 7.6 Trigger 接口

trigger 为外部数字输入。  
trigger 只表示操作者请求。  
trigger 不得直接驱动任何激光输出。  
当前版本默认占位使用 GPIO4，最终引脚与有效极性必须在板级确认后锁定为配置参数。

### 7.7 DAC 接口

DAC 通过 I2C 接入。  
一个通道用于 LD setpoint。  
一个通道用于 TEC setpoint。  
DAC 输出必须服从 supervisor 的安全策略。

## 8. 派生逻辑量定义

以下逻辑量由 monitor/supervisor 生成，不直接等同于某个原始 GPIO。

### 8.1 TEC_POWER_OK

为真当且仅当：

- PWR_TEC_GOOD 为有效
- 且最近一次更新未过期

### 8.2 LD_POWER_OK

为真当且仅当：

- PWR_LD_PGOOD 为有效
- 且最近一次更新未过期

### 8.3 TEC_DIGITAL_OK

为真当且仅当：

- TEC_TEMPGD 为有效
- 且最近一次更新未过期

### 8.4 LD_DIGITAL_OK

为真当且仅当：

- LD_LPGD 为有效
- 且最近一次更新未过期

### 8.5 ORIENTATION_OK

由 IMU monitor 根据姿态算法给出。  
为真当且仅当：

- IMU 初始化成功
- 数据更新未过期
- 当前姿态在允许范围内
- 姿态状态已满足最小稳定时间要求

### 8.6 TRIGGER_ACTIVE

由 trigger monitor 给出。  
为真当且仅当：

- trigger 输入经去抖后有效
- 且最近更新未过期

### 8.7 PERMIT_READY

为真当且仅当：

- TEC_POWER_OK
- LD_POWER_OK
- TEC_DIGITAL_OK
- LD_DIGITAL_OK
- ORIENTATION_OK
- 无 internal fault latch
- 无 usage fault latch

### 8.8 FIRE_REQUEST

等价于 TRIGGER_ACTIVE

### 8.9 EMISSION_ALLOWED

等价于：

`PERMIT_READY AND FIRE_REQUEST`

## 9. 状态机定义

状态机定义如下：

- OFF
- POWERUP_TEC
- POWERUP_LD
- READY
- FIRING
- FAULT_INTERNAL
- FAULT_USAGE

## 10. 各状态的强制输出定义

这是实现中最关键的一部分之一。  
每个状态必须有唯一、确定的输出目标。

### 10.1 OFF

- PWR_TEC_EN = 0
- PWR_LD_EN = 0
- LD_SBDN = shutdown
- DAC = safe values

### 10.2 POWERUP_TEC

- PWR_TEC_EN = 1
- PWR_LD_EN = 0
- LD_SBDN = shutdown
- DAC = safe values

### 10.3 POWERUP_LD

- PWR_TEC_EN = 1
- PWR_LD_EN = 1
- LD_SBDN = shutdown
- DAC = safe values

### 10.4 READY

- PWR_TEC_EN = 1
- PWR_LD_EN = 1
- LD_SBDN = standby
- DAC:
  TEC = ready/hold value
  LD = safe or non-firing standby value

### 10.5 FIRING

- PWR_TEC_EN = 1
- PWR_LD_EN = 1
- LD_SBDN = operate
- DAC:
  TEC = ready/hold value
  LD = active firing value

### 10.6 FAULT_INTERNAL

- PWR_TEC_EN = 0
- PWR_LD_EN = 0
- LD_SBDN = shutdown
- DAC = safe values

### 10.7 FAULT_USAGE

- PWR_TEC_EN = 1
- PWR_LD_EN = 1
- LD_SBDN = shutdown
- DAC = safe values

## 11. 上电策略

### 11.1 启动入口

MCU 启动后，系统必须先进入 OFF。

### 11.2 自动推进策略

当前版本规定：

只要系统未处于 fault 清除阻塞条件，固件应自动从 OFF 推进上电序列。

即默认路径为：

`OFF -> POWERUP_TEC -> POWERUP_LD -> READY`

不要求额外 arm 命令。

### 11.3 上电时序

必须按以下顺序执行：

1. 进入 OFF
2. 设置所有安全输出为默认安全值
3. 配置总线与监测模块
4. 进入 POWERUP_TEC
5. 置 PWR_TEC_EN = 1
6. 等待 PWR_TEC_GOOD
7. 成功后进入 POWERUP_LD
8. 置 PWR_LD_EN = 1
9. 等待 PWR_LD_PGOOD
10. 成功后进入 READY

## 12. 上电超时与失败

### 12.1 POWERUP_TEC 超时

若在 T_PWR_TEC_GOOD_TIMEOUT_MS 内未观察到稳定有效的 PWR_TEC_GOOD，必须：

- latch internal fault
- 转入 FAULT_INTERNAL

### 12.2 POWERUP_LD 超时

若在 T_PWR_LD_GOOD_TIMEOUT_MS 内未观察到稳定有效的 PWR_LD_PGOOD，必须：

- latch internal fault
- 转入 FAULT_INTERNAL

### 12.3 稳定判定

PGOOD 信号不得仅凭单次采样即视为成立。  
必须经过最小稳定窗口 T_GOOD_STABLE_MS。

## 13. READY 到 FIRING 的规则

从 READY 进入 FIRING 必须满足：

- PERMIT_READY == true
- TRIGGER_ACTIVE == true

并且上述条件应至少连续保持 T_FIRE_ENTRY_VALIDATE_MS。

如果条件不满足，则不得进入 FIRING。

## 14. FIRING 维持规则

系统仅在以下条件持续成立时保持 FIRING：

- TRIGGER_ACTIVE == true
- PERMIT_READY == true

以下任一条件丢失，必须退出 FIRING：

- trigger 释放
- TEC_POWER_OK 丢失
- LD_POWER_OK 丢失
- TEC_DIGITAL_OK 丢失
- LD_DIGITAL_OK 丢失
- ORIENTATION_OK 丢失
- fault latch 置位

退出后目标状态由 fault 分类决定。

## 15. Fault 分类规则

### 15.1 Internal Fault 来源

本版本必须支持以下来源：

- POWERUP_TEC 超时
- POWERUP_LD 超时
- 运行期 PWR_TEC_GOOD 丢失
- 运行期 PWR_LD_PGOOD 丢失
- 运行期 TEC_TEMPGD 丢失
- 运行期 LD_LPGD 丢失
- 关键监测源 freshness 超时
- DAC 关键写入失败（若发生于需要更新安全输出的状态切换）
- IMU 初始化失败或 IMU freshness 超时

### 15.2 Usage Fault 来源

本版本必须支持以下来源：

- ORIENTATION_OK 丢失（在 READY 或 FIRING 期间）

未来可加入：

- distance 超限
- 接触条件失效
- 外部互锁失败

## 16. Fault 反应规则

### 16.1 Internal Fault 反应

检测到 internal fault 后必须立即：

- 置 internal fault latch
- 进入 FAULT_INTERNAL
- 强制 LD_SBDN = shutdown
- 强制 DAC = safe
- 关闭 PWR_LD_EN
- 关闭 PWR_TEC_EN

### 16.2 Usage Fault 反应

检测到 usage fault 后必须立即：

- 置 usage fault latch
- 进入 FAULT_USAGE
- 强制 LD_SBDN = shutdown
- DAC = safe
- 保持两路电源域开启

### 16.3 Fault 优先级

若 internal 和 usage 同时出现，必须优先 internal。

## 17. Fault 清除与恢复

### 17.1 通用要求

两类 fault 清除前都必须满足：

- TRIGGER_ACTIVE == false
- 且 trigger 已连续释放至少 T_TRIGGER_RELEASE_CLEAR_MS

### 17.2 Usage Fault 清除

FAULT_USAGE 清除条件：

- usage 条件恢复有效
- trigger 已释放稳定
- 无 internal fault

满足后应自动返回 READY。

### 17.3 Internal Fault 清除

FAULT_INTERNAL 清除条件：

- trigger 已释放稳定
- internal fault 来源对应条件恢复为“可重新尝试”
- 满足当前版本允许的自动清除策略

清除后应先返回 OFF，再重新执行完整上电序列。

### 17.4 当前版本的 internal auto-clear 范围

当前版本允许以下 internal fault 自动清除后重新尝试：

- power-up timeout
- 短暂的 good 信号恢复正常
- IMU 重新初始化成功

当前版本暂不定义 service-only fault。

## 18. LD_SBDN 控制规范

### 18.1 语义

- shutdown：GPIO 输出低
- standby：GPIO 配置为输入/高阻
- operate：GPIO 输出高

### 18.2 约束

固件必须通过 GPIO 模式切换实现三态，不得通过“输出中间电压”模拟 standby。

### 18.3 切换顺序

以下顺序必须满足：

- 进入 FIRING 前，先确保 DAC 已写入 firing 目标，再切 LD_SBDN -> operate
- 退出 FIRING 时，先将 LD_SBDN -> shutdown，再执行 power disable（若目标状态要求断电）
- 任意 fault 状态下，LD_SBDN 必须优先于或至少不晚于 power disable 进入 shutdown

## 19. LD_PCN 控制规范

当前版本中：

- LD_PCN 固定配置为高支路选择
- 不参与动态状态机切换
- 初始化完成后保持固定输出
- 若未来支持多档发射模式，可扩展为 profile parameter

## 20. DAC 控制规范

### 20.1 DAC 角色

DAC 不是独立安全决策器，而是安全从属输出。

### 20.2 DAC 目标值类型

固件必须维护三组逻辑 DAC 目标：

- DAC_SAFE
- DAC_READY
- DAC_FIRING

其中每组包含：

- LD channel target
- TEC channel target

### 20.3 各状态 DAC 策略

- OFF / POWERUP_* / FAULT_*：使用 DAC_SAFE
- READY：使用 DAC_READY
- FIRING：使用 DAC_FIRING

### 20.4 写入要求

状态切换引发 DAC 目标变化时，固件必须：

- 写入目标值
- 检查 I2C 事务成功
- 若关键写入失败，则置 internal fault

### 20.5 默认值

DAC 的绝对码值属于校准参数，不由本文档给定固定数字，但必须通过配置表加载。

## 21. IMU 逻辑规范

### 21.1 目标

固件必须通过 IMU 判断设备是否处于允许的工作姿态。

### 21.2 输出

IMU monitor 必须输出：

- 最新姿态估计
- 更新时间戳
- ORIENTATION_OK

### 21.3 判定原则

当前版本要求：

- 设备设计为向下工作
- 当光束方向抬起超过允许阈值时，ORIENTATION_OK = false

具体阈值由配置参数给出，例如：

- ANGLE_MAX_UPWARD_DEG

### 21.4 稳定性要求

为避免单样本抖动，ORIENTATION_OK 的置位与清除都必须经过最小稳定窗口：

- T_ORIENTATION_ASSERT_MS
- T_ORIENTATION_CLEAR_MS

### 21.5 Freshness

若 IMU 数据超过 T_IMU_STALE_TIMEOUT_MS 未更新，则：

- ORIENTATION_OK = false
- 并触发 internal fault

## 22. Trigger 处理规范

### 22.1 语义

trigger 只表示操作者请求发射。

### 22.2 去抖

trigger 必须经过时间去抖后才允许生成 TRIGGER_ACTIVE。

配置参数：

- T_TRIGGER_DEBOUNCE_MS

### 22.3 FIRING 维持

FIRING 必须要求 trigger 持续有效。  
trigger 释放后必须退出 FIRING。

### 22.4 Fault 恢复保护

所有 fault 恢复前必须要求 trigger 已释放并保持稳定：

- T_TRIGGER_RELEASE_CLEAR_MS

## 23. Freshness 规范

每个关键监测源必须带时间戳。  
关键监测源包括但不限于：

- power good monitors
- ld digital monitor
- tec digital monitor
- imu monitor
- trigger monitor

若任一关键源超过其 freshness timeout，则该源必须被视为无效。

对无效源的处理：

- 若该源参与 permit，则 permit 视为 false
- 若该源属于系统可信运行的前提，则应触发 internal fault

## 24. 快照一致性规范

Supervisor 每次运行时必须读取一个逻辑一致的系统快照。  
禁止在一次决策中混用不同 monitor 在不同时间点的半更新变量。

推荐实现方式：

- 每个 monitor 输出自己的 snapshot struct 与更新时间戳
- supervisor 在单一时刻复制所有子快照到本地 decision snapshot
- 基于该 snapshot 计算 permit、fault、next state、outputs

## 25. 任务模型规范

### 25.1 必需任务

建议至少实现以下任务：

- task_monitor_digital
- task_monitor_trigger
- task_monitor_imu
- task_monitor_analog（当前版本可空实现或仅采集）
- task_supervisor
- task_logging（低优先级）

### 25.2 优先级原则

- supervisor 优先级最高或接近最高
- digital / trigger monitor 高于一般通信与日志
- logging 最低
- 不允许低优先级任务阻塞安全决策路径

### 25.3 周期原则

推荐：

- digital / trigger：更快
- imu：中速
- analog：较慢
- supervisor：固定快速周期

具体周期由配置参数定义，不在本文档写死数值。

## 26. 中断使用规范

### 26.1 当前版本允许的中断

- 可选 trigger 边沿中断
- 可选 IMU_INT2 中断
- 可选关键数字 fault pin 边沿中断

### 26.2 中断职责

ISR 只能做以下事情：

- 记录事件
- 更新时间戳
- 置位标志
- 通知任务

ISR 不得：

- 执行复杂状态机
- 做总线事务
- 直接完成完整 fault 逻辑
- 长时间阻塞

### 26.3 最终裁决

即使存在 ISR，最终状态转移与输出控制仍必须由 supervisor/state-machine 完成。

## 27. 核心输出 ownership 规范

只有 output/state layer 可以直接写以下核心安全输出：

- PWR_TEC_EN
- PWR_LD_EN
- LD_SBDN
- DAC 安全/激活值

monitor 任务不得直接修改这些输出。

## 28. 参数表要求

固件必须提供集中配置参数表，至少包含：

- trigger 极性
- trigger debounce 时间
- trigger release-clear 时间
- TEC power-good timeout
- LD power-good timeout
- good stable 时间
- supervisor 周期
- IMU stale timeout
- IMU assert/clear stable 时间
- orientation 阈值
- DAC safe/ready/firing 目标值
- 各 freshness timeout

## 29. 日志与可观测性

固件应支持以下日志事件：

- 上电序列状态变化
- fault 置位与清除
- permit 丢失原因
- firing 进入与退出原因
- IMU 初始化与 stale
- DAC 写失败
- power-good timeout

日志不得阻塞安全任务。  
必要时应采用环形缓冲区异步输出。

## 30. 启动默认安全要求

MCU 上电、复位、异常重启期间，固件初始化必须尽快设置以下默认行为：

- PWR_TEC_EN = 0
- PWR_LD_EN = 0
- LD_SBDN = shutdown
- DAC safe

若在固件尚未运行前存在短暂不确定窗口，硬件默认电路必须保证激光不会被误使能。

## 31. 最小验收行为

实现完成后，固件至少应满足以下验收行为：

- 上电后不会直接发射
- 上电序列按 TEC→LD 顺序进行
- 任一 PGOOD 超时会进入 FAULT_INTERNAL
- READY 状态下 LD_SBDN = standby
- 只有 permit && trigger 才进入 FIRING
- trigger 释放立即退出 FIRING
- 姿态不安全会触发 FAULT_USAGE
- FAULT_USAGE 不断电，但禁止发射
- FAULT_INTERNAL 必须断电
- fault 清除前 trigger 必须释放
- internal fault 恢复需重走完整上电序列
- stale monitor 数据会 fail safe

## 32. 仍开放项

以下项目在本规范中保留为配置或未来扩展项：

- trigger 最终物理引脚与极性
- distance sensor 集成方式
- 模拟监测阈值与换算
- GPIO37 共享节点最终策略
- IMU 具体姿态算法实现细节
- 是否为关键数字 fault 加硬中断旁路

## 33. 总结

本固件是一个针对手持 NIR 激光设备的实时监督安全控制器。  
其核心设计原则如下：

- 分离 power supervision 与 emission permit
- 所有激光发射都必须通过 supervisor 与状态机批准
- internal 与 usage fault 必须分类处理
- 故障恢复必须受控，不允许按住 trigger 自动重入 firing
- 任何不确定性都必须落到安全输出
- 采样、快照、监督、输出必须职责分离
- 所有核心安全输出必须由统一层管理

## 四、我对你们当前设计逻辑的最终判断

我的结论是：

你们当前的设计逻辑是“正确且成熟的”

不是方向错了，而是已经到了需要“工程收口”的阶段。

最大的问题不是“逻辑错”

而是：

- 还有少量歧义
- 还缺少强制实现级细节
- 还缺少参数化、ownership、状态输出唯一性这些工程约束

我最建议你们立刻补强的 5 件事：

- 把 trigger 语义写死
- 把 OFF 是否自动推进到 power-up 写死
- 把 LD_SBDN 三态实现机制写死
- 把 freshness timeout / debounce / timeout 参数集中列成表
- 做一张 fault source → reaction → recovery 的明确表
