#include "oled_display.h"
#include "assets/lang_config.h"
#include "alarm_clock_driver.h"  // 添加头文件

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>
#include "application.h"

#define TAG "OledDisplay"

LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    width_ = width;
    height_ = height;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;//LVGL task stack size
    port_cfg.timer_period_ms = 40;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }

    // 初始化闹钟UI驱动
    alarm_ui_driver_ = std::make_unique<AlarmUIDriver>();
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();

    // 清理闹钟UI驱动
    alarm_ui_driver_.reset();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetAlarmManager(AlarmManager* manager) {
    alarm_manager_ = manager;
    SetupAlarmUIDriver();
}

void OledDisplay::SetupAlarmUIDriver() {
    if (!alarm_ui_driver_ || !alarm_manager_) {
        return;
    }
    
    AlarmUIDriver::AlarmConfig config;
    config.alarm_manager = alarm_manager_;
    config.display_width = width_;
    config.display_height = height_;
    config.display = this;
    config.on_alarm_save = [this](const Alarm& alarm) {
        SaveAlarmCallback(alarm);
    };
    
    config.on_alarm_delete = [this](int alarm_id) {
        DeleteAlarmCallback(alarm_id);
    };
    
    config.on_alarm_toggle = [this](int alarm_id, bool enabled) {
        ToggleAlarmCallback(alarm_id, enabled);
    };
    
    config.on_ui_exit = [this]() {
        ExitAlarmUICallback();
    };

    if (!alarm_ui_driver_->Initialize(config)) {
        ESP_LOGE(TAG, "Failed to initialize alarm UI driver");
    }
    ESP_LOGI(TAG,"HELLO!! set alarm ui driver");
}

void OledDisplay::ShowAlarmList() {
    if (!alarm_ui_driver_ || !alarm_manager_) {
        ESP_LOGE(TAG, "Alarm UI driver not ready");
        return;
    }
    
    DisplayLockGuard lock(this);
    
    // 保存当前屏幕状态
    if (!alarm_ui_active_) {
        previous_screen_ = lv_scr_act();
        alarm_ui_active_ = true;
    }

    ESP_LOGE(TAG, "HELLO!!  alarm_ui_active_=%d container_=%0x", alarm_ui_active_, container_);
    // 隐藏主UI元素
    if (container_) {
        lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 显示闹钟列表
    alarm_ui_driver_->ShowAlarmList();
}

void OledDisplay::ShowAlarmEdit(int alarm_id) {
    if (!alarm_ui_driver_ || !alarm_manager_) {
        return;
    }
    
    Alarm* alarm_to_edit = nullptr;
    if (alarm_id >= 0) {
        // 查找现有闹钟
        for (int i = 0; i < alarm_manager_->count; i++) {
            if (alarm_manager_->alarms[i].id == alarm_id) {
                alarm_to_edit = &alarm_manager_->alarms[i];
                break;
            }
        }
    }
    
    DisplayLockGuard lock(this);
    alarm_ui_driver_->ShowAlarmEdit(alarm_to_edit);
}

void OledDisplay::ShowAlarmAlert(const Alarm* alarm) {
    if (!alarm_ui_driver_) {
        return;
    }
    
    DisplayLockGuard lock(this);
    alarm_ui_driver_->ShowAlarmAlert(alarm);
}

void OledDisplay::HideAlarmUI() {
    if (alarm_ui_active_) {
        DisplayLockGuard lock(this);
        RestoreMainUI();
    }
}

void OledDisplay::RestoreMainUI() {
    // 显示主UI元素
    if (container_) {
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }

    // 加载之前的屏幕
    if (previous_screen_) {
        lv_scr_load(previous_screen_);
    }

    // 通知驱动清理
    if (alarm_ui_driver_) {
        alarm_ui_driver_->ExitAlarmUI();
    }
    
    alarm_ui_active_ = false;
    previous_screen_ = nullptr;
}

void OledDisplay::UpdateAlarmStatus() {
    if (alarm_ui_driver_) {
        alarm_ui_driver_->RefreshAlarmList();
    }
}

void OledDisplay::SetAlarmButtonState(uint32_t key_id) {
    if (alarm_ui_driver_) {
        alarm_button_state_ = key_id;
    }
}

void OledDisplay::HandleKeyEvent(uint32_t key_id) {
    // 如果闹钟UI激活，先让闹钟UI处理按键
    if (alarm_ui_active_ && alarm_ui_driver_) {
        ESP_LOGI(TAG, "HELLO!! handle key_id=%d", key_id);
        // SetAlarmButtonState(key_id);
        auto& app = Application::GetInstance();
        app.Schedule([this, key_id]() {
            if (alarm_ui_driver_->HandleKeyPress(key_id)) {
                ESP_LOGI(TAG, "HELLO!! process Alarm UI handled key press key_id=%d", key_id);
                return; // 按键已被处理
            }
            // switch(alarm_button_state_)
            // {
            //     case kAlarmButtonStateDisabled:
            //         break;
            //     case kAlarmButtonStateNormal:
            //         break;
            //     case kAlarmButtonStatePressed:
            //         break;
            // }
        });
    }

    // 否则由主UI处理
    // ... 现有的按键处理逻辑 ...
}

void OledDisplay::SaveAlarmCallback(const Alarm& alarm) {
    if (alarm_save_callback_) {
        alarm_save_callback_(alarm);
    }
    
    // 保存后返回列表
    // ShowAlarmList();
    UpdateAlarmStatus();
}

void OledDisplay::DeleteAlarmCallback(int alarm_id) {
    if (alarm_delete_callback_) {
        alarm_delete_callback_(alarm_id);
    }
    
    // 删除后刷新列表
    UpdateAlarmStatus();
}

void OledDisplay::ToggleAlarmCallback(int alarm_id, bool enabled) {
    if (alarm_toggle_callback_) {
        alarm_toggle_callback_(alarm_id, enabled);
    }
    
    // 切换后刷新列表
    UpdateAlarmStatus();
}

void OledDisplay::ExitAlarmUICallback() {
    HideAlarmUI();
    
    if (alarm_ui_exit_callback_) {
        alarm_ui_exit_callback_();
    }
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // 如果闹钟UI激活，不更新聊天消息
    if (alarm_ui_active_) {
        return;
    }

    // 替换所有换行符为空格
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // vTaskDelay(pdMS_TO_TICKS(9000));
    // ESP_LOGI(TAG, "HELLO!! setup ui!!");
    // // 创建新屏幕
    // current_screen_ = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(current_screen_, config_.display_width, config_.display_height);
    // lv_obj_set_style_bg_color(current_screen_, lv_color_white(), 0);

    // // 标题
    // lv_obj_t* title = lv_label_create(current_screen_);
    // lv_label_set_text(title, "Alarms");
    // lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
}

void OledDisplay::SetAlarmSaveCallback(std::function<void(const Alarm&)> callback) {
    alarm_save_callback_ = std::move(callback);
}

void OledDisplay::SetAlarmDeleteCallback(std::function<void(int)> callback) {
    alarm_delete_callback_ = std::move(callback);
}

void OledDisplay::SetAlarmToggleCallback(std::function<void(int, bool)> callback) {
    alarm_toggle_callback_ = std::move(callback);
}

void OledDisplay::SetAlarmUIExitCallback(std::function<void()> callback) {
    alarm_ui_exit_callback_ = std::move(callback);
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

