#include "alarm_clock_driver.h"
#include "alarm_clock_menu.h"
#include <esp_log.h>
#include <cstdio>      // 添加这一行（如果还没有）
#include "application.h"
#include "display/display.h"  // 添加这一行

#define TAG "AlarmUIDriver"

LV_FONT_DECLARE(font_awesome_14_1);
LV_FONT_DECLARE(font_puhui_14_1);
// 全局变量
AlarmUIDriver* AlarmUIDriver::instance_ = nullptr;
AlarmManager* g_alarm_manager = nullptr;

// 辅助函数声明
extern "C" {
    // void ui_create_alarm_list_screen(void);
    void ui_switch_to_edit_screen(Alarm* alarm_to_edit);
    void ui_show_alarm_alert(const Alarm* alarm);
    void ui_alarm_manager_register(AlarmManager* alarm_manager);
}

AlarmUIDriver::AlarmUIDriver() {
    instance_ = this;
}

AlarmUIDriver::~AlarmUIDriver() {
    if (current_screen_) {
        lv_obj_del(current_screen_);
    }
    instance_ = nullptr;
}

bool AlarmUIDriver::Initialize(const AlarmConfig& config) {
    if (initialized_) {
        return true;
    }

    config_ = config;

    // 设置全局闹钟管理器（兼容现有C代码）
    if (config_.alarm_manager) {
        g_alarm_manager = config_.alarm_manager;
        // ui_alarm_manager_register(config_.alarm_manager);
    }

    initialized_ = true;
    last_handle_time = esp_log_timestamp();
    ESP_LOGI(TAG, "Alarm UI driver initialized");
    return true;
}

void AlarmUIDriver::ShowAlarmList() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Driver not initialized");
        return;
    }

    ESP_LOGE(TAG, "HELLO!! Driver show alarm list");
    CreateAlarmListScreen();
}

void AlarmUIDriver::ShowAlarmEdit(Alarm* alarm) {
    if (!initialized_) return;

    // CreateAlarmEditScreen(alarm);
}

void AlarmUIDriver::ShowAlarmAlert(const Alarm* alarm) {
    if (!initialized_) return;

    CreateAlarmAlertScreen(alarm);
}

void AlarmUIDriver::ExitAlarmUI() {
    if (current_screen_) {
        lv_obj_del(current_screen_);
        current_screen_ = nullptr;
        list_container_ = nullptr;
    }
}

void AlarmUIDriver::RefreshAlarmList() {
    if (list_container_ && g_alarm_manager) {
        ESP_LOGI(TAG, "HELLO!! Refreshing alarm list");
        RefreshAlarmListView();
    }
}

bool AlarmUIDriver::HandleKeyPress(uint32_t key_id) {
    if (!initialized_ || !current_screen_) {
        return false;
    }

    uint32_t now = esp_log_timestamp();
    if (now - last_handle_time < MIN_HANDLE_INTERVAL_MS) {
        ESP_LOGW("AlarmUIDriver", "Key handled too fast, ignored. last_handle_time=%0x now=%0x", last_handle_time, now);
        return false; // 丢弃过快的按键
    }
    last_handle_time = now;

    ESP_LOGI(TAG, "HELLO!! key pressed id=%d", key_id);
    // 处理闹钟UI的按键事件
    switch (key_id) {
        case 0x01:  // KEY_BACK
            if (config_.on_ui_exit) {
                config_.on_ui_exit();
            }
            return true;

        case 0x02:  // KEY_DOWN
            if(g_alarm_manager->count != 0)
            {
                focused_index_ = (focused_index_ + 1) % g_alarm_manager->count;
            }
            else
            {
                focused_index_ = 0;
            }
            ESP_LOGI(TAG, "HELLO!!! Focused index: %d conut=%d", focused_index_, g_alarm_manager->count);
            RefreshAlarmList();
            break;
        case 0x03:  // KEY_UP
            break;
        case 0x04:  // KEY_ENTER
            {
                // 改为使用兼容方法：
                lv_obj_t* focused_obj = nullptr;

                // 方法1：通过焦点组查找（如果使用了焦点组）
                lv_group_t* group = lv_obj_get_group(list_container_);
                if (group) {
                    focused_obj = lv_group_get_focused(group);
                }

                // 方法2：如果没找到，遍历容器查找（备用方法）
                if (!focused_obj) {
                    for (int i = 0; i < lv_obj_get_child_cnt(list_container_); i++) {
                        lv_obj_t* child = lv_obj_get_child(list_container_, i);
                        if (child && lv_obj_has_state(child, LV_STATE_FOCUSED)) {
                            focused_obj = child;
                            break;
                        }
                    }
                }

                // ⭐⭐⭐ 关键：检查焦点对象是否有效 ⭐⭐⭐
                if (!focused_obj) {
                    ESP_LOGW(TAG, "No focused object found, cannot send click event");
                    return false;  // 或者执行其他错误处理
                }

                // 安全地获取用户数据
                void* user_data_ptr = lv_obj_get_user_data(focused_obj);
                if (user_data_ptr) {
                    // 安全地转换为整数（假设存储的是整数）
                    int alarm_id_value = *(int *)user_data_ptr;
                    int alarm_id = (int)alarm_id_value;
                    ESP_LOGI(TAG, "Focused object alarm_id=%d", alarm_id);
                } else {
                    ESP_LOGW(TAG, "No user data in focused object");
                }

                // 方法A：使用 lv_obj_send_event（兼容新旧版本）
                #if LVGL_VERSION_MAJOR >= 8
                    lv_obj_send_event(focused_obj, LV_EVENT_CLICKED, this);
                #else
                    // 旧版本可能需要触发点击状态
                    lv_obj_add_state(focused_obj, LV_STATE_PRESSED);
                    lv_event_send(focused_obj, LV_EVENT_CLICKED, NULL);
                    lv_obj_clear_state(focused_obj, LV_STATE_PRESSED);
                #endif
            }

            // 这些按键可以在具体的UI控件中处理
            return true;
        default:
            return false;
    }
    return true;
}

// ============ 内部UI创建方法 ============

void AlarmUIDriver::CreateAlarmListScreen() {
    // 清理现有屏幕
    if (current_screen_) {
        ESP_LOGI(TAG, "HELLO!!! creat alarm=%d %d", LV_HOR_RES, LV_VER_RES);
        lv_obj_del(current_screen_);
    }

    // 创建新屏幕
    // current_screen_ = lv_obj_create(lv_scr_act());
    current_screen_ = lv_obj_create(NULL);
    if (!current_screen_) {
        ESP_LOGE(TAG, "创建屏幕对象失败！");
        return;
    }
    // // 先获取当前活动屏幕的尺寸
    // lv_obj_t* active_screen = lv_scr_act();
    // lv_coord_t screen_width = lv_obj_get_width(active_screen);
    // lv_coord_t screen_height = lv_obj_get_height(active_screen);

    // ESP_LOGI(TAG, "HELLO!!! creat alarm screen. Active screen size: %dx%d",
    //          screen_width, screen_height);
    ESP_LOGI(TAG, "HELLO!!Config display size: %dx%d",
             config_.display_width, config_.display_height);

    ESP_LOGI(TAG, "屏幕对象创建成功: %p", current_screen_);
    lv_obj_set_size(current_screen_, config_.display_width, config_.display_height);
    lv_obj_set_style_text_font(current_screen_, &font_puhui_14_1, 0);

    ESP_LOGI(TAG, "设置屏幕尺寸: %dx%d", config_.display_width, config_.display_height);

    lv_obj_set_style_bg_color(current_screen_, lv_color_white(), 0);
    ESP_LOGI(TAG, "设置背景颜色为黑色");

    // 立即设置红色边框以便调试
    lv_obj_set_style_border_color(current_screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(current_screen_, 1, 0);
    lv_obj_set_style_radius(current_screen_, 0, 0);


    // // 标题
    // lv_obj_t* title = lv_label_create(current_screen_);
    // if (title) {
    //     lv_obj_set_size(title, 40, 16);
    //     lv_label_set_text(title, "Alarms");
    //     lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    //     lv_obj_set_style_text_color(title, lv_color_white(), 0);
    //     ESP_LOGI(TAG, "标题创建成功");
    // } else {
    //     ESP_LOGE(TAG, "标题创建失败！");
    // }
    // // 立即刷新显示
    // lv_refr_now(NULL);
    // ESP_LOGI(TAG, "强制刷新显示");

    // 列表容器
    list_container_ = lv_obj_create(current_screen_);
    lv_obj_set_size(list_container_, config_.display_width - 8,
                   38);
    lv_obj_align(list_container_, LV_ALIGN_TOP_MID, 0, 4);
    // lv_obj_align_to(list_container_, title, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    // lv_obj_set_style_text_color(list_container_, lv_color_white(), 0);

    // 返回按钮
    lv_obj_t* btn_back = lv_btn_create(current_screen_);
    lv_obj_set_size(btn_back, 40, 16);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    // lv_obj_align_to(btn_back, btn_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_black(), 0);
    lv_obj_set_style_margin_top(btn_back, 6, 0);
    // lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_t* back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, "返回");
    lv_obj_center(back_label);
    lv_obj_set_style_text_color(back_label,  lv_color_white(), 0);
    lv_obj_add_event_cb(btn_back, OnExitButtonClicked, LV_EVENT_CLICKED, this);

    // 添加闹钟按钮
    lv_obj_t* btn_add = lv_btn_create(current_screen_);
    // lv_obj_set_size(btn_add, 60, 25);
    lv_obj_set_size(btn_add, 40, 16);
    lv_obj_align(btn_add, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
    // lv_obj_align_to(btn_add, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_color(btn_add, lv_color_black(), 0);
    lv_obj_set_style_text_color(btn_add,  lv_color_white(), 0);
    lv_obj_set_style_margin_top(btn_add, 6, 0);

    lv_obj_t* btn_label = lv_label_create(btn_add);
    lv_label_set_text(btn_label, "添加");
    lv_obj_center(btn_label);
    lv_obj_add_event_cb(btn_add, OnAddButtonClicked, LV_EVENT_CLICKED, this);

    // 闹钟编辑界面
    edit_container_ = lv_obj_create(current_screen_);
    lv_obj_set_size(edit_container_, config_.display_width,
                   config_.display_height);
    // ⭐⭐⭐ 关键：创建后立即隐藏 ⭐⭐⭐
    lv_obj_add_flag(edit_container_, LV_OBJ_FLAG_HIDDEN);

    // 刷新列表
    RefreshAlarmListView();

    // 加载屏幕
    lv_scr_load(current_screen_);
    // 再次强制刷新
    lv_refr_now(NULL);
    ESP_LOGI(TAG, "HELLO!!! creat alarm list screen");
}
#define EDIT_CANCEL "NULL"
#define EDIT_OK "NULL"

//TODO
void AlarmUIDriver::CreateTimePicker(lv_obj_t* parent, Alarm* alarm) {
    // 调试信息
    ESP_LOGI(TAG, "创建时间选择器,屏幕尺寸:128x64");
    
    // 解析当前时间
    int hour = 8, minute = 0;
    if (alarm && alarm->time[0]) {
        sscanf(alarm->time, "%d:%d", &hour, &minute);
    }
    
    // 限制值范围
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;
    
    // ==================== 1. 小时选择器 ====================
    lv_obj_t* hour_roller = lv_roller_create(parent);
    if (!hour_roller) {
        ESP_LOGE(TAG, "创建小时选择器失败");
        return;
    }
    
    // 设置尺寸和位置（适应128x64屏幕）
    lv_obj_set_size(hour_roller, 48, 36);  // 缩小尺寸
    lv_obj_set_pos(hour_roller, 10, 14);   // 调整位置
    
    // 使用静态字符串常量（节省栈内存）
    static const char* HOUR_OPTIONS = 
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n"
        "20\n21\n22\n23";

    lv_roller_set_options(hour_roller, HOUR_OPTIONS, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(hour_roller, hour, LV_ANIM_OFF);
    lv_obj_set_user_data(hour_roller, (void*)EDIT_HOUR);
    
    // ==================== 2. 冒号标签 ====================
    lv_obj_t* colon_label = lv_label_create(parent);
    if (colon_label) {
        lv_label_set_text(colon_label, ":");
        lv_obj_set_pos(colon_label, 63, 24);  // 居中位置
        lv_obj_set_style_text_font(colon_label, &font_puhui_14_1, 0);
    }
    
    // ==================== 3. 分钟选择器 ====================
    lv_obj_t* minute_roller = lv_roller_create(parent);
    if (!minute_roller) {
        ESP_LOGE(TAG, "创建分钟选择器失败");
        return;
    }
    
    lv_obj_set_size(minute_roller, 48, 36);
    lv_obj_set_pos(minute_roller, 70, 14);
    
    // 分钟选项（5分钟间隔）
    static const char* MINUTE_OPTIONS = 
        "00\n05\n10\n15\n20\n25\n30\n35\n40\n45\n50\n55";
    
    lv_roller_set_options(minute_roller, MINUTE_OPTIONS, LV_ROLLER_MODE_NORMAL);

    // 计算最近的5分钟间隔
    int minute_index = minute / 5;
    if (minute_index >= 12) minute_index = 11;
    lv_roller_set_selected(minute_roller, minute_index, LV_ANIM_OFF);
    lv_obj_set_user_data(minute_roller, (void*)EDIT_MINUTE);
    
    // ==================== 4. 样式优化 ====================
    
    // 小时选择器样式
    lv_obj_set_style_text_line_space(hour_roller, 1, LV_PART_MAIN);     // 最小行距
    lv_obj_set_style_text_font(hour_roller, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_style_text_align(hour_roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hour_roller, LV_OPA_30, LV_PART_MAIN);
    
    lv_obj_set_style_bg_color(hour_roller, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(hour_roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(hour_roller, lv_color_black(), LV_PART_MAIN);

    // 选中项样式
    lv_obj_set_style_bg_color(hour_roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(hour_roller, lv_color_white(), LV_PART_SELECTED);
    
    // 分钟选择器样式（复制小时样式）
    lv_obj_set_style_text_line_space(minute_roller, 1, LV_PART_MAIN);
    lv_obj_set_style_text_font(minute_roller, &font_puhui_14_1, LV_PART_MAIN);
    lv_obj_set_style_text_align(minute_roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(minute_roller, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(minute_roller, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(minute_roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(minute_roller, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(minute_roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(minute_roller, lv_color_white(), LV_PART_SELECTED);

    // ==================== 5. 添加确定/取消按钮 ====================
    int btn_width = 52;
    int btn_height = 24;
    int btn_y = 56;  // 底部位置

    // 取消按钮
    lv_obj_t* cancel_btn = lv_btn_create(parent);
    lv_obj_set_size(cancel_btn, btn_width, btn_height);
    lv_obj_set_pos(cancel_btn, 10, btn_y);

    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "取消");
    lv_obj_center(cancel_label);
    lv_obj_set_user_data(cancel_btn, (void*)EDIT_CANCEL);

    // 确定按钮
    lv_obj_t* ok_btn = lv_btn_create(parent);
    lv_obj_set_size(ok_btn, btn_width, btn_height);
    lv_obj_set_pos(ok_btn, 66, btn_y);

    lv_obj_t* ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "确定");
    lv_obj_center(ok_label);
    lv_obj_set_user_data(ok_btn, (void*)EDIT_OK);

    // 按钮样式
    lv_obj_set_style_bg_color(cancel_btn, lv_color_black(), 0);
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(ok_btn, lv_color_black(), 0);
    lv_obj_set_style_text_color(ok_label, lv_color_white(), 0);

    // ==================== 6. 调试信息 ====================
    ESP_LOGI(TAG, "时间选择器创建完成：小时=%d,分钟=%d,索引%d,", 
             hour, minute, minute_index);
    ESP_LOGI(TAG, "控件位置：小时(%d,%d)，分钟(%d,%d)", 
             10, 14, 70, 14);

    // 重要：删除 lv_refr_now(NULL) - 由LVGL自动管理
}

void AlarmUIDriver::CreateAlarmEditScreen(Alarm* alarm) {
    if (current_screen_) {
        if(edit_container_)
        {
            lv_obj_clean(edit_container_);
            lv_obj_clear_flag(edit_container_, LV_OBJ_FLAG_HIDDEN);
            CreateTimePicker(edit_container_, alarm);

            // 移到最前面（如果需要）
            // lv_obj_move_foreground(edit_container_);
        }
        else
        {
            ESP_LOGE(TAG, "HELLO!!! edit_container_ is null");
        }
    }
}


void AlarmUIDriver::CreateAlarmAlertScreen(const Alarm* alarm) {
    // 使用现有的C函数
    ui_show_alarm_alert(alarm);
}

// ============ 辅助函数 ============

void AlarmUIDriver::DaysToString(int days_mask, char* buf, size_t buf_len) {
    const char* day_chars = "SMTWTFS";
    int pos = 0;
    buf[0] = '\0';

    if (days_mask == 0b01111111) {
        snprintf(buf, buf_len, "Every day");
        return;
    }

    for (int i = 0; i < 7; i++) {
        if (days_mask & (1 << i)) {
            if (pos > 0) buf[pos++] = ' ';
            buf[pos++] = day_chars[i];
        }
    }

    if (pos == 0) {
        snprintf(buf, buf_len, "Once");
    } else {
        buf[pos] = '\0';
    }
}

void AlarmUIDriver::RefreshAlarmListView() {
    if (!list_container_ || !g_alarm_manager) {
        return;
    }
    if (config_.display)
    {
        DisplayLockGuard lock(config_.display);
        uint32_t start = esp_log_timestamp();
        // ESP_LOGI(TAG, "HELLO!! refresh alarm menu=%d lv_pct(95)=%d", g_alarm_manager->count, lv_pct(95));

        // 清除现有内容
        lv_obj_clean(list_container_);

        // 设置容器为可滚动
        lv_obj_set_scrollbar_mode(list_container_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_scroll_dir(list_container_, LV_DIR_VER);  // 允许垂直滚动

        // 设置容器高度为所有闹钟项的总高度（动态计算）
        int total_height = g_alarm_manager->count * (16); // 每个按钮30px + 2px间距
        // int screen_height = config_.display_height - 22; // 减去标题和按钮区域
        int screen_height = 34;

        if (total_height > screen_height) {
            lv_obj_set_height(list_container_, screen_height);
        } else {
            lv_obj_set_height(list_container_, total_height);
        }

        // 设置垂直布局
        lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(list_container_, LV_FLEX_ALIGN_START, 
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(list_container_, 2, 0); // 将不同行的控件间距设为2像素（原来是默认值）

        int max_visible_items = 3;  // 屏幕高度34，每个项18px(16+2)，最多3个
        // int display_count = (g_alarm_manager->count > max_visible_items) ? 
        //                     max_visible_items : g_alarm_manager->count;
        int display_count = g_alarm_manager->count;
        char item_text[64];
        char days_str[16];
        if(focused_index_ >= g_alarm_manager->count)
        {
            focused_index_ = focused_index_ % g_alarm_manager->count;
        }
        // 添加闹钟项
        // for (int i = 0; i < g_alarm_manager->count; i++) {
        for (int i = (focused_index_ % display_count); i < display_count; i++) {
            Alarm* alarm = &g_alarm_manager->alarms[i];

            // ESP_LOGI(TAG, "HELLO!!! alarm=%d %s %s %s", alarm->id, alarm->time, alarm->message, alarm->repeat ? "Repeat" : "Once");
            DaysToString(alarm->days_of_week, days_str, sizeof(days_str));

            snprintf(item_text, sizeof(item_text), "%d %s %s %s",
                    i,
                    alarm->enabled ? "开" : "关",  // 使用图标更直观
                    alarm->time,
                    alarm->repeat ? days_str : "Once");
            ESP_LOGI(TAG, "HELLO!!! alarm=%d %s cnt=%d idx=%d", alarm->id, item_text, display_count, focused_index_);
            // 创建列表项按钮
            lv_obj_t* item_btn = lv_btn_create(list_container_);
            lv_obj_set_width(item_btn, 120);
            lv_obj_set_height(item_btn, 16);
            lv_obj_set_style_bg_color(item_btn, lv_color_black(), 0);
            lv_obj_set_style_text_color(item_btn,  lv_color_white(), 0);

            //使用Flex布局实现左对齐
            lv_obj_set_flex_flow(item_btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item_btn, LV_FLEX_ALIGN_START,  // 主轴起点对齐（左对齐）
                                LV_FLEX_ALIGN_CENTER,          // 交叉轴居中
                                LV_FLEX_ALIGN_CENTER);

            // 设置焦点导航属性
            lv_obj_add_flag(item_btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

            // 第一个项目默认聚焦
            if (i == (focused_index_ % display_count)) {
                lv_obj_add_state(item_btn, LV_STATE_FOCUSED);
                // focused_index_ = 0;
                ESP_LOGI(TAG, "HELLO!!! item_btn=%0x i=%d cnt=%d", item_btn, i, display_count);
            }
            // else
            // {
            //     lv_obj_clear_state(item_btn, LV_STATE_DEFAULT);
            // }

            lv_obj_t* label = lv_label_create(item_btn);
            lv_label_set_text(label, item_text);
            // lv_obj_set_style_pad_left(label, 0, 0);  // 添加左边距

            // lv_obj_set_style_pad_left(label, 4, 0);  // 添加左边距
            // lv_obj_center(label);
            // lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
            // lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 4, -2);

            // 存储闹钟ID和索引
            lv_obj_set_user_data(item_btn, (void*)&alarm->id);

            // 添加点击事件
            lv_obj_add_event_cb(item_btn, OnListItemClicked, LV_EVENT_CLICKED, this);

            // 添加间距
            // lv_obj_set_style_margin_bottom(item_btn, 0, 0);
        }
        uint32_t end = esp_log_timestamp();
        // 获取当前焦点对象
        // lv_obj_t* focused_obj = lv_group_get_focused_obj(list_container_);
        
        {
            // 改为使用兼容方法：
            lv_group_t* group = lv_obj_get_group(list_container_);
            lv_obj_t* focused_obj = group ? lv_group_get_focused(group) : nullptr;

            // 或者如果 list_container_ 本身不是焦点组，而是容器：
            // 遍历容器内的对象找到焦点状态的对象
            for (int i = 0; i < lv_obj_get_child_cnt(list_container_); i++) {
                lv_obj_t* child = lv_obj_get_child(list_container_, i);
                int alarm_id = *(int *)lv_obj_get_user_data(child);
                ESP_LOGI(TAG, "HELLO!!! lv_obj_get_state=%0x alarm_id=%d", lv_obj_get_state(child), alarm_id);
                if (lv_obj_has_state(child, LV_STATE_FOCUSED)) {
                    focused_obj = child;
                    // break;
                }
            }

            ESP_LOGI(TAG, "HELLO!!! RefreshAlarmListView took %d ms, focused_obj=%0x", end - start, focused_obj);
        }
    }
}

// ============ 静态事件处理函数 ============

void AlarmUIDriver::OnSaveAlarm(lv_event_t* e) {
    if (!instance_) return;

    // 这里需要从事件中提取编辑数据
    // 实际实现中，您需要从UI控件获取数据

    EditContext* ctx = (EditContext*)lv_event_get_user_data(e);
    if (!ctx) return;

    // 创建闹钟对象
    Alarm new_alarm;

    // 从ctx中获取数据填充new_alarm
    // ...

    // 调用保存回调
    if (instance_->config_.on_alarm_save) {
        instance_->config_.on_alarm_save(new_alarm);
    }

    // 返回列表
}

void AlarmUIDriver::OnCancelEdit(lv_event_t* e) {
    if (instance_) {
    }
}

void AlarmUIDriver::OnCloseAlert(lv_event_t* e) {
    if (instance_) {
    }
}

void AlarmUIDriver::OnListItemClicked(lv_event_t* e) {
    if (!instance_) return;


    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int alarm_id = *(int *)lv_obj_get_user_data(btn);
    ESP_LOGI(TAG, "HELLO!! OnListItemClicked: alarm_id=%d", alarm_id);

    if (instance_->config_.alarm_manager) {
        AlarmManager* mgr = instance_->config_.alarm_manager;
        Alarm* found_alarm = nullptr;

        for (int i = 0; i < mgr->count; i++) {
            if (mgr->alarms[i].id == alarm_id) {
                found_alarm = &mgr->alarms[i];
                break;
            }
        }

        if (found_alarm) {
            instance_->CreateAlarmEditScreen(found_alarm);
        }
    }
}

void AlarmUIDriver::OnAddButtonClicked(lv_event_t* e) {
    if (instance_) {
        // instance_->CreateAlarmEditScreen(nullptr);
    }
}

void AlarmUIDriver::OnExitButtonClicked(lv_event_t* e) {
    if (instance_ && instance_->config_.on_ui_exit) {
        instance_->config_.on_ui_exit();
    }
}

void AlarmUIDriver::RegisterAlarmManager(AlarmManager* manager) {
    g_alarm_manager = manager;
}