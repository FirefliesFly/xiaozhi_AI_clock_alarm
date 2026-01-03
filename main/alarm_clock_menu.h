// ui_alarm.h
#ifndef UI_ALARM_H
#define UI_ALARM_H

// #include "lvgl/lvgl.h"
// #include "alarm_clock.h" // 假设您的闹钟管理器头文件
#include <lvgl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 闹钟结构体
typedef struct {
    int id;                 // 闹钟ID
    char time[6];           // 时间格式 HH:MM
    char message[50];       // 闹钟提示信息
    bool enabled;           // 是否启用
    bool repeat;            // 是否重复
    int days_of_week;       // 重复的星期几 (位掩码 0b01111111 表示周一到周日)
} Alarm;

// 闹钟管理器结构体
typedef struct {
    Alarm* alarms;          // 闹钟数组
    int capacity;           // 数组容量
    int count;              // 当前闹钟数量
    int next_id;            // 下一个可用的ID
    int enable;            // 下一个可用的ID
} AlarmManager;

typedef struct {
    lv_obj_t* roller_hour;
    lv_obj_t* roller_min;
    lv_obj_t* btnm_days;
    lv_obj_t* sw_enable;
    Alarm* alarm_ptr;
} EditContext;

// 声明全局管理器引用
extern AlarmManager* g_alarm_manager;

// 界面函数
// void ui_create_alarm_list_screen(void);
void ui_switch_to_edit_screen(Alarm* alarm_to_edit);
void ui_show_alarm_alert(const Alarm* alarm);
void ui_alarm_manager_register(AlarmManager* alarm_manager);
#ifdef __cplusplus
}
#endif

#endif