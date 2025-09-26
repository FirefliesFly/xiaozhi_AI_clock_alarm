#include <sys/time.h> 
#include "alarm_clock.h"
#include <time.h>
#include <esp_log.h>

#include "esp_heap_caps.h"

#define TAG "alarm_clock"


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
} AlarmManager;

// 函数声明
AlarmManager* alarm_manager_create(int initial_capacity);
void alarm_manager_destroy(AlarmManager* manager);
int alarm_create(AlarmManager* manager, const char* time, const char* message, bool repeat, int days);
bool alarm_delete(AlarmManager* manager, int id);
void alarm_display_all(const AlarmManager* manager);
void alarm_check(const AlarmManager* manager);
bool is_time_match(const char* current_time, const char* alarm_time);
int get_current_weekday();

alarm_clock_ring_func_t alarm_clock_ring = NULL;

AlarmManager* alarm_mgr = NULL;

/*************************外部接口函数********************* */
int alarm_clock_init() {
    // 创建闹钟管理器
    alarm_mgr = alarm_manager_create(10);
    
    if (!alarm_mgr) {
        ESP_LOGI(TAG, "创建闹钟管理器失败!\n");
        return 1;
    }
    
    ESP_LOGI(TAG, "多闹钟管理系统\n");
    
    // 创建一些示例闹钟
    alarm_create(alarm_mgr, "07:30", "起床时间", true, 0b01111100); // 周一到周五
    alarm_create(alarm_mgr, "12:00", "午餐时间", true, 0b01111111); // 每天
    // alarm_create(alarm_mgr, "18:00", "下班时间", true, 0b01111100); // 周一到周五
    // alarm_create(alarm_mgr, "19:49", "睡觉时间", true, 0b01111111); // 每天
    
    // 显示所有闹钟
    alarm_display_all(alarm_mgr);
    
    // 模拟检查闹钟（在实际应用中应该放在循环中）
    ESP_LOGI(TAG, "\n模拟检查闹钟:\n");
    alarm_check(alarm_mgr);
    
    // 删除一个闹钟
    // ESP_LOGI(TAG, "\n删除ID为2的闹钟...\n");
    // if (alarm_delete(alarm_mgr, 2)) {
    //     ESP_LOGI(TAG, "删除成功!\n");
    // } else {
    //     ESP_LOGI(TAG, "删除失败，未找到该ID!\n");
    // }
  
    // if (alarm_delete(alarm_mgr, 2)) {
    //     ESP_LOGI(TAG, "删除成功!\n");
    // } else {
    //     ESP_LOGI(TAG, "删除失败，未找到该ID!\n");
    // }

    // alarm_create(alarm_mgr, "12:00", "午餐时间", true, 0b01111111); // 每天

    // 再次显示所有闹钟
    alarm_display_all(alarm_mgr);
    
    // 清理资源
    //alarm_manager_destroy(alarm_mgr);
    
    return 0;
}

// 模拟检查闹钟（在实际应用中应该放在循环中）
void alarm_clock_check() { 
    if(alarm_mgr){
        ESP_LOGI(TAG, "\n模拟检查闹钟:\n");
        alarm_check(alarm_mgr);
    }
    else{
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
    }
}

bool alarm_create_callback(const char* time, const char* message, bool repeat, int days)
{
    if(alarm_mgr == NULL)
    {
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
        return false;
    }
    else{
        bool ret;
        if(alarm_create(alarm_mgr, time, message, repeat, days) == -1){
            ret = false;
        }
        else{
            ret = true;
        }
        alarm_display_all(alarm_mgr);
        return ret;
    }
}

bool alarm_delete_callback(int id){
    if(alarm_mgr == NULL)
    {
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
        return false;
    }
    else{
        bool ret = alarm_delete(alarm_mgr, id);
        alarm_display_all(alarm_mgr);
        return ret;
    }
}

uint8_t get_alarm_counts()
{
    if(alarm_mgr == NULL)
    {
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
        return 0;
    }
    else{
        return alarm_mgr->count;
    }
}
/***************************************************************************************************************888 */
// 创建闹钟管理器
AlarmManager* alarm_manager_create(int initial_capacity) {
    AlarmManager* manager = (AlarmManager*)heap_caps_malloc(sizeof(AlarmManager), MALLOC_CAP_DEFAULT);
    if (!manager) return NULL;
    
    manager->alarms = (Alarm*)heap_caps_malloc(initial_capacity * sizeof(Alarm), MALLOC_CAP_DEFAULT);
    if (!manager->alarms) {
        heap_caps_free(manager);
        return NULL;
    }
    
    manager->capacity = initial_capacity;
    manager->count = 0;
    manager->next_id = 1;
    return manager;
}

// 销毁闹钟管理器
void alarm_manager_destroy(AlarmManager* manager) {
    if (manager) {
        heap_caps_free(manager->alarms);
        heap_caps_free(manager);
    }
}

// 创建新闹钟
int alarm_create(AlarmManager* manager, const char* time, const char* message, bool repeat, int days) {
    if (manager->count >= manager->capacity) {
        // 扩容
        // int new_capacity = manager->capacity * 2;
        // Alarm* new_alarms = (Alarm*)realloc(manager->alarms, new_capacity * sizeof(Alarm));
        // if (!new_alarms) return -1;
        
        // manager->alarms = new_alarms;
        // manager->capacity = new_capacity;
        return -1;
    }
    
    Alarm* new_alarm = &manager->alarms[manager->count];
    new_alarm->id = manager->next_id++;
    strncpy(new_alarm->time, time, sizeof(new_alarm->time) - 1);
    new_alarm->time[5] = '\0';
    strncpy(new_alarm->message, message, sizeof(new_alarm->message) - 1);
    new_alarm->enabled = true;
    new_alarm->repeat = repeat;
    new_alarm->days_of_week = days;
    
    manager->count++;
    return new_alarm->id;
}

// 删除闹钟
bool alarm_delete(AlarmManager* manager, int id) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            // 将后一个元素移到当前位置
            if (i < manager->count - 1) {
                for(; i < manager->count - 1; i++){
                    manager->alarms[i] = manager->alarms[i + 1];
                    manager->alarms[i].id = i + 1;
                }
                //manager->alarms[i] = manager->alarms[manager->count - 1];
            }
            manager->count--;
            manager->next_id--;
            return true;
        }
    }
    return false;
}

// 显示所有闹钟
void alarm_display_all(const AlarmManager* manager) {
    ESP_LOGI(TAG, "\n=== 闹钟列表 ===\n");
    ESP_LOGI(TAG, "ID\t时间\t状态\t重复\t信息\n");
    ESP_LOGI(TAG, "-----------------------------------------\n");
    
    for (int i = 0; i < manager->count; i++) {
        const Alarm* alarm = &manager->alarms[i];
        ESP_LOGI(TAG, "%d\t%s\t%s\t%s\t%s\n",
               alarm->id,
               alarm->time,
               alarm->enabled ? "启用" : "禁用",
               alarm->repeat ? "是" : "否",
               alarm->message);
    }
    ESP_LOGI(TAG, "总共: %d 个闹钟\n", manager->count);
}

// 检查并触发闹钟
void alarm_check(const AlarmManager* manager) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    char current_time[6];
    strftime(current_time, sizeof(current_time), "%H:%M", tm_info);
    
    int current_weekday = get_current_weekday();
    
    for (int i = 0; i < manager->count; i++) {
        const Alarm* alarm = &manager->alarms[i];
        
        if (alarm->enabled && is_time_match(current_time, alarm->time)) {
            // 检查是否重复闹钟或者今天是设定的日期
            if (alarm->repeat || (alarm->days_of_week & (1 << current_weekday))) {
                ESP_LOGI(TAG, "\n⏰ 闹钟触发! [%s] %s\n", alarm->time, alarm->message);
                if(alarm_clock_ring != NULL)
                {
                    alarm_clock_ring();
                }
            }
        }
    }
}

// 检查时间是否匹配
bool is_time_match(const char* current_time, const char* alarm_time) {
    return strcmp(current_time, alarm_time) == 0;
}

// 获取当前星期几 (0=周日, 1=周一, ..., 6=周六)
int get_current_weekday() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    return tm_info->tm_wday; // 周日为0
}





// std::string Ml307Board::GetAlarmClockCountJson() {
//     /*
//      * 返回设备状态JSON
//      * 
//      * 返回的JSON结构如下：
//      * {
//      *     "alarm_clock_counts": {
//      *         "number": 2
//      *     },
//      * }
//      */
//     auto& board = Board::GetInstance();
//     auto root = cJSON_CreateObject();

//     // alarm_clock_counts
//     auto alarm_clock_counts = cJSON_CreateObject();
//     auto audio_codec = board.GetAudioCodec();
//     if (audio_codec) {
//         cJSON_AddNumberToObject(alarm_clock_counts, "number", get_alarm_counts());
//     }
//     cJSON_AddItemToObject(root, "alarm_clock_counts", alarm_clock_counts);

//     auto json_str = cJSON_PrintUnformatted(root);
//     std::string json(json_str);
//     cJSON_free(json_str);
//     cJSON_Delete(root);
//     return json;
// }

// typedef void (*func1)(void);
// typedef void (*func2)(uint8_t hour, uint8_t minute);
// typedef struct {
//     uint8_t hour;
//     uint8_t minute;
//     uint8_t wday;
//     func1 clock_delete;
//     func1 clock_create;
//     func2 clock_change;
// } AlarmClock_t;

// #define ALARM_CLOCK_MAX_COUNT 10
// AlarmClock_t alarm_clock[ALARM_CLOCK_MAX_COUNT];
// uint8_t alarm_clock_count = 0;
// uint8_t alarm_clock_index = 0;
// uint8_t alarm_clock_state = 0;

// void alarm_clock_init() {
//     for (uint8_t i = 0; i < ALARM_CLOCK_MAX_COUNT; i++) {
//         alarm_clock[i].hour = 0;
//         alarm_clock[i].minute = 0;
//         alarm_clock[i].wday = 0;
//         alarm_clock[i].clock_delete = NULL;
//         alarm_clock[i].clock_create = NULL;
//     }
// }

void alarm_clock_test() { 
    time_t now;
    struct tm time_tmp;
    time(&now);
    localtime_r(&now, &time_tmp);
    ESP_LOGI(TAG, "year: %u mounth: %u day: %u hour: %u min: %u sec: %u wday: %u", 
    time_tmp.tm_year + 1900, time_tmp.tm_mon + 1,time_tmp.tm_mday,time_tmp.tm_hour,time_tmp.tm_min,time_tmp.tm_sec, time_tmp.tm_wday);
}

