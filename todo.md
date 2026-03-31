1. Auxiliary host 到 ESP32 的最终落点
AUX_I2C_SDA_GPIO
AUX_I2C_SCL_GPIO
AUX_GPIO_INT_A_GPIO

2. Trigger 的最终系统定义 ✅
SW1 是否就是唯一 trigger
判定规则是不是必须 GPA0 && GPA1 同时低才算按下

3. IMU 机械安装语义
激光轴在 IMU 坐标系里是哪根轴
src/tasks/imu_task.c:18
src/modules/imu_monitor.h:47

4. DAC 的真实数值
DAC_LD_CODE_SAFE_TBD #安全状态下关断输出 0？
DAC_TEC_CODE_SAFE_TBD 
DAC_TEC_CODE_READY_TBD # TEC工作设定温度
DAC_LD_CODE_FIRING_TBD # LD 工作设定电流激光强度

5. DAC80502 的最终 I2C 地址 ✅
DAC_I2C_ADDR_7BIT_TBD
0x48

6. Auxiliary 上 TLC59116 / DRV2605 的最终 I2C 地址 ✅
AUX_TLC59116_ADDR_7BIT_TBD 0x60
AUX_DRV2605_ADDR_7BIT_TBD 0x5A

7. STUSB4500 基础状态寄存器和 bit 定义
STUSB4500_REG_ALERT_STATUS_1_TBD 读“告警/事件”状态的寄存器地址
STUSB4500_REG_PORT_STATUS_0_TBD 读端口基础状态的寄存器地址
STUSB4500_REG_PORT_STATUS_1_TBD 读端口/PD 合同相关状态的寄存器地址
STUSB4500_ALERT_FAULT_MASK_TBD 在 alert/status 寄存器里，哪些 bit 要算 fault
STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD 哪个 bit 表示 attached
STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD 哪个 bit 表示 contract valid

8. LD 模拟量链路
LD_LPGD 极性硬件确认 ✅
LD_TMO 的温度换算公式/板级缩放 ✅
LD_LIO 的电流换算公式/板级缩放 ✅
安全阈值

9. TEC 模拟量链路
TEC_TEMPGD 极性确认 ✅
TEC_TMO 温度换算 
TEC_ITEC 电流换算 ✅
TEC_VTEC 电压换算 scaling
安全阈值

10. ADC 真正接入
现在模拟量逻辑的结构有了，但 ADC 还没真正闭合。
用 ESP32 哪个 ADC unit/channel
每个模拟输入的衰减/量程

11. 模拟量是否进入 permit / fault
LD/TEC 模拟量只是监控？

12. PD 是否进入更高层策略
当前 PD 只是监督，不参与主安全闭环。
pd_contract_valid 是否影响 READY
pd_fault 是否算 internal fault

13. Distance sensor

14. 其他占位系统信号
LASER_GATE_GPIO
STATUS_LED_GPIO
LD_FAULT_N_GPIO
TEC_FAULT_N_GPIO
ESTOP_N_GPIO

15. Fault clear 的最终策略
internal clear 是否只要求 trigger release + IMU safe
是否还要加稳定时间
是否某些 internal fault 需要 service clear
这是策略层未完全锁死项。