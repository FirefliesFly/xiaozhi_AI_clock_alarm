#ifndef ALARM_UI_DRIVER_H
#define ALARM_UI_DRIVER_H

#include "alarm_clock_menu.h"
#include <lvgl.h>
#include <functional>  // 添加这一行
#include <cstdio>      // 添加这一行

// 最小UI响应间隔
#define MIN_HANDLE_INTERVAL_MS 50

// 在类定义中添加枚举
enum EditField {
    EDIT_TIME,
    EDIT_ENABLED,
    EDIT_REPEAT,
    EDIT_DAYS_LABEL,
    EDIT_DAYS_CONTAINER,
    EDIT_MESSAGE,
    EDIT_HOUR,
    EDIT_MINUTE
};

// 前向声明 Display 类
class Display;

class AlarmUIDriver {
public:
    // 闹钟UI事件回调定义
    using AlarmSaveCallback = std::function<void(const Alarm&)>;
    using AlarmDeleteCallback = std::function<void(int alarm_id)>;
    using AlarmToggleCallback = std::function<void(int alarm_id, bool enabled)>;
    using AlarmUIExitCallback = std::function<void()>;

    struct AlarmConfig {
        AlarmManager* alarm_manager = nullptr;
        int display_width = 128;
        int display_height = 64;
        Display* display = nullptr;  // 添加显示指针用于锁
        AlarmSaveCallback on_alarm_save;
        AlarmDeleteCallback on_alarm_delete;
        AlarmToggleCallback on_alarm_toggle;
        AlarmUIExitCallback on_ui_exit;
    };
    
    AlarmUIDriver();
    ~AlarmUIDriver();
    
    // 初始化闹钟UI驱动
    bool Initialize(const AlarmConfig& config);
    
    // UI界面控制
    void ShowAlarmList();
    void ShowAlarmEdit(Alarm* alarm = nullptr);
    void ShowAlarmAlert(const Alarm* alarm);
    void ExitAlarmUI();
    
    // 刷新UI
    void RefreshAlarmList();
    
    // 处理物理按键
    bool HandleKeyPress(uint32_t key_id);
    
    // 获取当前屏幕
    lv_obj_t* GetCurrentScreen() const { return current_screen_; }
    
    // 静态方法供C代码调用
    static void OnSaveAlarm(lv_event_t* e);
    static void OnCancelEdit(lv_event_t* e);
    static void OnCloseAlert(lv_event_t* e);
    static void OnListItemClicked(lv_event_t* e);
    static void OnAddButtonClicked(lv_event_t* e);
    static void OnExitButtonClicked(lv_event_t* e);
    
    // 注册闹钟管理器（用于C代码）
    static void RegisterAlarmManager(AlarmManager* manager);

    // 添加公共静态方法
    static AlarmUIDriver* GetInstance() { return instance_; }
    // static AlarmConfig* GetConfig() { return &config_; }

private:
    static AlarmUIDriver* instance_;  // 保持为私有
    AlarmConfig config_;
    bool initialized_ = false;
    uint32_t last_handle_time;
    int focused_index_ = 0;  // 添加这行

    lv_obj_t* current_screen_ = nullptr;
    lv_obj_t* list_container_ = nullptr;
    lv_obj_t* edit_container_ = nullptr;

    // 内部UI创建方法
    void CreateAlarmListScreen();
    void CreateTimePicker(lv_obj_t* parent, Alarm* alarm);
    void CreateAlarmEditScreen(Alarm* alarm);
    void CreateAlarmAlertScreen(const Alarm* alarm);

    // 辅助函数
    void DaysToString(int days_mask, char* buf, size_t buf_len);
    void RefreshAlarmListView();
};

#endif // ALARM_UI_DRIVER_H