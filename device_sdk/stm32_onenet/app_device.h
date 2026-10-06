#ifndef __APP_DEVICE_H
#define __APP_DEVICE_H

#include "main.h"

/* ============================================================
 *  业务层：设备物模型
 *
 *  - 处理云端下行（属性设置 LED/buzzer/fan、命令下发）→ 调 BSP 驱动
 *  - 组装上行内容（温湿度上报）
 *  - 工程里**唯一**使用 cJSON 的模块：通用层只搬字符串
 * ============================================================ */

/* 注册下行处理表 + 初始化可控外设（风扇引脚等） */
void Device_Init(void);

/* 读 DHT11 当前值并按物模型格式上报 */
void Device_ReportTempHumi(void);

/* 把执行器（灯/蜂鸣器/风扇）当前状态回报给平台，面板才能显示真实状态。
 * 只上报「收到过下发」的项（= 物模型里确实存在），所以可以放心调用。*/
void Device_ReportActuatorState(void);

#endif /* __APP_DEVICE_H */
