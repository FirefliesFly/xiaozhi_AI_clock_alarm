// alarm_clock_menu.c
#include "alarm_clock_menu.h"
#include "alarm_clock_driver.h"
#include <stdio.h>
#include <esp_log.h>

#define TAG "alarm_clock_menu"

extern AlarmManager* g_alarm_manager;

// 静态变量
static Alarm* s_editing_alarm = NULL;
static lv_obj_t* s_list_screen = NULL;
static lv_obj_t* s_edit_screen = NULL;

static void days_to_string(int days_mask, char* buf, size_t buf_len);

// 1. 辅助函数：将星期位掩码转换为字符串
static void days_to_string(int days_mask, char* buf, size_t buf_len) {
    const char* day_chars = "日一二三四五六";
    int pos = 0;
    buf[0] = '\0';
    
    // 简化显示：如果是一周每天，显示"每天"
    if(days_mask == 0b01111111) {
        lv_strncpy(buf, "每天", buf_len);
        return;
    }
    
    // 遍历星期
    for(int i = 0; i < 7; i++) {
        if(days_mask & (1 << i)) {
            int char_len = lv_snprintf(&buf[pos], buf_len - pos, "%c", day_chars[i]);
            if(char_len > 0) pos += char_len;
        }
    }
    if(pos == 0) {
        lv_strncpy(buf, "单次", buf_len);
    }
}

// // 5. 创建闹钟列表界面
// void ui_create_alarm_list_screen(void) {
//     // 如果有驱动实例，使用驱动的创建方法
//     AlarmUIDriver* driver = AlarmUIDriver::GetInstance();
//     if (driver) {
//         driver->ShowAlarmList();
//         return;
//     }
// }

// 6. 编辑界面（保持不变，但修正事件回调函数）
// void ui_switch_to_edit_screen(Alarm* alarm_to_edit) {
//     // 如果有驱动实例，使用驱动的编辑方法
//     AlarmUIDriver* driver = AlarmUIDriver::GetInstance();
//     if (driver) {
//         driver->ShowAlarmEdit(alarm_to_edit);
//         return;
//     }
// }

// 9. 全屏提醒界面（修正字体问题）
void ui_show_alarm_alert(const Alarm* alarm) {
    // 如果有驱动实例，使用驱动的提醒方法
    AlarmUIDriver* driver = AlarmUIDriver::GetInstance();
    if (driver) {
        driver->ShowAlarmAlert(alarm);
        return;
    }
}