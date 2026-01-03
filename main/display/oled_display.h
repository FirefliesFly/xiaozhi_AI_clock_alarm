#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"
#include "alarm_clock_menu.h"
#include <functional>  // 添加这一行

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// 前置声明
class AlarmUIDriver;

enum alarm_button_state_tag {
    kAlarmButtonStateNormal,
    kAlarmButtonStatePressed,
    kAlarmButtonStateDisabled
};

class OledDisplay : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    DisplayFonts fonts_;

    // 闹钟UI相关成员
    std::unique_ptr<AlarmUIDriver> alarm_ui_driver_;
    AlarmManager* alarm_manager_ = nullptr;
    bool alarm_ui_active_ = false;
    uint32_t alarm_button_state_;
    lv_obj_t* previous_screen_ = nullptr;

    // 闹钟事件回调
    std::function<void(const Alarm&)> alarm_save_callback_;
    std::function<void(int)> alarm_delete_callback_;
    std::function<void(int, bool)> alarm_toggle_callback_;
    std::function<void()> alarm_ui_exit_callback_;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();
    
    // 闹钟UI驱动相关方法
    void SetupAlarmUIDriver();
    void SaveAlarmCallback(const Alarm& alarm);
    void DeleteAlarmCallback(int alarm_id);
    void ToggleAlarmCallback(int alarm_id, bool enabled);
    void ExitAlarmUICallback();
    
    // 恢复主UI
    void RestoreMainUI();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y,
                DisplayFonts fonts);
    ~OledDisplay();

    virtual void SetChatMessage(const char* role, const char* content) override;
    
    // 闹钟UI接口
    void ShowAlarmList();
    void ShowAlarmEdit(int alarm_id = -1); // -1表示创建新闹钟
    void ShowAlarmAlert(const Alarm* alarm);
    void HideAlarmUI();
    void UpdateAlarmStatus();
    void SetAlarmButtonState(uint32_t key_id);
    // 设置闹钟管理器
    void SetAlarmManager(AlarmManager* manager);

    // 设置闹钟事件回调
    void SetAlarmSaveCallback(std::function<void(const Alarm&)> callback);
    void SetAlarmDeleteCallback(std::function<void(int)> callback);
    void SetAlarmToggleCallback(std::function<void(int, bool)> callback);
    void SetAlarmUIExitCallback(std::function<void()> callback);
    
    // 处理按键事件
    virtual void HandleKeyEvent(uint32_t key_id);
    
    // 获取当前状态
    bool IsAlarmUIActive() const { return alarm_ui_active_; }
};

#endif // OLED_DISPLAY_H