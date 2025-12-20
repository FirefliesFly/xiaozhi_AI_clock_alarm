#include <sys/time.h> 
#include "alarm_clock.h"
#include <time.h>
#include <esp_log.h>

#include "esp_heap_caps.h"
#include "esp_assert.h"
#include "settings.h"
#include <cJSON.h>
#define TAG "alarm_clock"

#define ALARM_MAX_COUNT 10

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

// 函数声明
AlarmManager* alarm_manager_create(int initial_capacity);
void alarm_manager_destroy(AlarmManager* manager);
int alarm_create(AlarmManager* manager, const char* time, const char* message, bool repeat, int days);
bool alarm_delete(AlarmManager* manager, int id);
void alarm_display_all(const AlarmManager* manager);
void alarm_check(const AlarmManager* manager);
bool is_time_match(const char* current_time, const char* alarm_time);
int get_current_weekday();

// 新增的接口函数声明
bool alarm_modify(AlarmManager* manager, int id, const char* time, const char* message, bool enabled, bool repeat, int days_of_week);
bool alarm_modify_time(AlarmManager* manager, int id, const char* time);
bool alarm_modify_message(AlarmManager* manager, int id, const char* message);
bool alarm_enable(AlarmManager* manager, int id, bool enable);
bool alarm_modify_repeat(AlarmManager* manager, int id, bool repeat, int days);
void alarm_save_to_nvs(AlarmManager* manager);

// 外部回调接口声明
bool alarm_modify_callback(int id, const char* time, const char* message, bool enabled, bool repeat, int days);
bool alarm_enable_callback(int id, bool enable);

alarm_clock_ring_func_t alarm_clock_ring = NULL;

AlarmManager* alarm_mgr = NULL;

bool CheckAlarmManagerFromNv(void) {
    Settings settings("AlarmManager", false);
    bool AlarmManager_enable = settings.GetBool("enable",  false);// default to false

    return AlarmManager_enable;
}

int GetAlarmCountFromNv(void) {

    if(CheckAlarmManagerFromNv() == false)
    {
        return 0;
    }

    Settings settings("AlarmManager", false);
    int count = settings.GetInt("count",  0);// default to 0

    return count;
}

int GetAlarmCapacityFromNv(void) {

    if(CheckAlarmManagerFromNv() == false)
    {
        ESP_LOGI(TAG, "Deafaut AlarmManager capacity: 10");
        return 10;// default to 10
    }

    Settings settings("AlarmManager", false);
    int capacity = settings.GetInt("capacity",  10);// default to 10

    std::string jsonData = settings.GetString("AlarmInfo", "");
    ESP_LOGI(TAG, "原始JSON数据: %s", jsonData.c_str());
    ESP_LOGI(TAG, "AlarmManager capacity: %d", capacity);

    if(capacity > ALARM_MAX_COUNT || capacity < 0)
    {
        Settings settings("AlarmManager", true);
        settings.SetInt("capacity", 10);
        capacity = settings.GetInt("capacity",  10);// default to 10
        ESP_LOGI(TAG, "Init AlarmManager capacity: %d", capacity);
        TRY_STATIC_ASSERT(capacity <= ALARM_MAX_COUNT && capacity >= 0, "capacity out of range");
    }
    return capacity;
}

// {
//     "AlarmManager": 
//     {
//     "count": 2, 
//     "capacity": 10,
//     "next_id": 3,
//     "enable": true,
//     "AlarmInfo": 
//     {
//       "Alarm": [
//         {"id": 1, "time": "07:30", "message": "起床时间", "enabled": true, "repeat": true, "days_of_week": 60}, 
//         {"id": 2, "time": "12:00", "message": "起床时间", "enabled": true, "repeat": true, "days_of_week": 60}
//       ]
//     }
//     }
// }
void GetAlarmManagerFromNv(AlarmManager* manager) {
    if(CheckAlarmManagerFromNv() == false) {
        manager->enable = false;
        return;
    }

    manager->enable = true;
    manager->capacity = GetAlarmCapacityFromNv();
    manager->count = GetAlarmCountFromNv();
    manager->next_id = manager->count + 1;

    Settings settings("AlarmManager", false);
    std::string jsonData = settings.GetString("AlarmInfo", "");
    std::string AlarmManager_jsonData = settings.GetString("AlarmManager", "");
    ESP_LOGI(TAG, "AlarmManager原始JSON数据: %s", AlarmManager_jsonData.c_str());
    ESP_LOGI(TAG, "AlarmInfo原始JSON数据: %s", jsonData.c_str());

    if (!jsonData.empty()) {
        cJSON *root = cJSON_Parse(jsonData.c_str());
        if (root) {
            cJSON *alarmArray = cJSON_GetObjectItem(root, "Alarm");
            if (alarmArray && cJSON_IsArray(alarmArray)) {
                int arraySize = cJSON_GetArraySize(alarmArray);
                
                // 确保不超过容量限制
                if (arraySize <= manager->capacity) {
                    manager->count = arraySize;
                    
                    // 解析每个闹钟项
                    for (int i = 0; i < arraySize; i++) {
                        cJSON *alarmItem = cJSON_GetArrayItem(alarmArray, i);
                        if (alarmItem) {
                            Alarm *alarm = &manager->alarms[i];
                            
                            cJSON *idItem = cJSON_GetObjectItem(alarmItem, "id");
                            if (idItem && cJSON_IsNumber(idItem)) {
                                alarm->id = idItem->valueint;
                            }
                            
                            cJSON *timeItem = cJSON_GetObjectItem(alarmItem, "time");
                            if (timeItem && cJSON_IsString(timeItem)) {
                                strncpy(alarm->time, timeItem->valuestring, sizeof(alarm->time) - 1);
                                alarm->time[sizeof(alarm->time) - 1] = '\0';
                            }
                            
                            cJSON *messageItem = cJSON_GetObjectItem(alarmItem, "message");
                            if (messageItem && cJSON_IsString(messageItem)) {
                                strncpy(alarm->message, messageItem->valuestring, sizeof(alarm->message) - 1);
                                alarm->message[sizeof(alarm->message) - 1] = '\0';
                            }
                            
                            cJSON *enabledItem = cJSON_GetObjectItem(alarmItem, "enabled");
                            if (enabledItem && cJSON_IsBool(enabledItem)) {
                                alarm->enabled = cJSON_IsTrue(enabledItem);
                            } else {
                                alarm->enabled = true; // 默认启用
                            }
                            
                            cJSON *repeatItem = cJSON_GetObjectItem(alarmItem, "repeat");
                            if (repeatItem && cJSON_IsBool(repeatItem)) {
                                alarm->repeat = cJSON_IsTrue(repeatItem);
                            } else {
                                alarm->repeat = false; // 默认不重复
                            }
                            
                            cJSON *daysItem = cJSON_GetObjectItem(alarmItem, "days_of_week");
                            if (daysItem && cJSON_IsNumber(daysItem)) {
                                alarm->days_of_week = daysItem->valueint;
                            } else {
                                alarm->days_of_week = 0; // 默认无重复日期
                            }
                        }
                    }
                } else {
                    ESP_LOGW(TAG, "Alarm count exceeds capacity: %d > %d", arraySize, manager->capacity);
                }
            } else {
                ESP_LOGW(TAG, "Failed to parse alarm array from JSON");
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "Failed to parse JSON data: %s", jsonData.c_str());
        }
    }
}


/*************************外部接口函数********************* */
int alarm_clock_init() {
    // 创建闹钟管理器
    alarm_mgr = alarm_manager_create(GetAlarmCapacityFromNv());
    
    if (!alarm_mgr) {
        ESP_LOGI(TAG, "创建闹钟管理器失败!\n");
        return 1;
    }
    
    ESP_LOGI(TAG, "多闹钟管理系统。闹钟个数：%d\n", alarm_mgr->count);

    // 创建一些示例闹钟
    // alarm_create(alarm_mgr, "07:30", "起床时间", true, 0b01111100);// 周一到周五

    // alarm_create(alarm_mgr, "12:00", "午餐时间", true, 0b01111111); // 每天
    // alarm_create(alarm_mgr, "18:00", "下班时间", true, 0b01111100); // 周一到周五
    // alarm_create(alarm_mgr, "19:49", "睡觉时间", true, 0b01111111); // 每天
    TRY_STATIC_ASSERT(alarm_mgr->count <= ALARM_MAX_COUNT && alarm_mgr->count >= 0, "alarm count must be between 0 and ALARM_MAX_COUNT");
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
            // 保存到NVS（这一步是必需的，如果希望数据持久化）
            alarm_save_to_nvs(alarm_mgr);
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

    ESP_LOGI(TAG, "创建闹钟管理器成功，初始容量为%d\n", initial_capacity);
    GetAlarmManagerFromNv(manager);

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
/**
 * 位掩码规则说明
在代码中，days_of_week 字段使用一个整数来表示一周中的哪些天需要触发闹钟，每一位对应一周中的一天：

位位置:  7    6    5    4    3    2    1    0
星期:    -    日   一   二   三   四   五   六
但实际上根据代码中的注释和使用情况，应该是：

位位置:  7    6    5    4    3    2    1    0
星期:    -    六   五   四   三   二   一   日
具体示例
周一到周五 (0b01111100 或 60)

二进制: 01111100
对应星期: 周一(位2)、周二(位3)、周三(位4)、周四(位5)、周五(位6)
计算: 4+8+16+32 = 60
每天 (0b01111111 或 127)

二进制: 01111111
对应星期: 周日到周六所有天
计算: 1+2+4+8+16+32+64 = 127
周末 (0b00000011 或 3)

二进制: 00000011
对应星期: 周日(位0)、周六(位1)
计算: 1+2 = 3
特定几天 (例如周三和周五 = 0b00010100 或 20)

二进制: 00010100
对应星期: 周三(位4)、周五(位6)
计算: 8+32 = 40
 */
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


// 修改指定ID的闹钟信息
bool alarm_modify(AlarmManager* manager, int id, const char* time, const char* message, bool enabled, bool repeat, int days_of_week) {
    if (!manager) return false;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            // 更新闹钟信息
            if (time) {
                strncpy(manager->alarms[i].time, time, sizeof(manager->alarms[i].time) - 1);
                manager->alarms[i].time[sizeof(manager->alarms[i].time) - 1] = '\0';
            }
            
            if (message) {
                strncpy(manager->alarms[i].message, message, sizeof(manager->alarms[i].message) - 1);
                manager->alarms[i].message[sizeof(manager->alarms[i].message) - 1] = '\0';
            }
            
            manager->alarms[i].enabled = enabled;
            manager->alarms[i].repeat = repeat;
            manager->alarms[i].days_of_week = days_of_week;
            
            return true;
        }
    }
    return false;
}

// 仅修改闹钟时间
bool alarm_modify_time(AlarmManager* manager, int id, const char* time) {
    if (!manager || !time) return false;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            strncpy(manager->alarms[i].time, time, sizeof(manager->alarms[i].time) - 1);
            manager->alarms[i].time[sizeof(manager->alarms[i].time) - 1] = '\0';
            return true;
        }
    }
    return false;
}

// 仅修改闹钟消息
bool alarm_modify_message(AlarmManager* manager, int id, const char* message) {
    if (!manager || !message) return false;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            strncpy(manager->alarms[i].message, message, sizeof(manager->alarms[i].message) - 1);
            manager->alarms[i].message[sizeof(manager->alarms[i].message) - 1] = '\0';
            return true;
        }
    }
    return false;
}

// 启用或禁用指定ID的闹钟
bool alarm_enable(AlarmManager* manager, int id, bool enable) {
    if (!manager) return false;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            manager->alarms[i].enabled = enable;
            return true;
        }
    }
    return false;
}

// 修改闹钟重复设置
bool alarm_modify_repeat(AlarmManager* manager, int id, bool repeat, int days) {
    if (!manager) return false;
    
    for (int i = 0; i < manager->count; i++) {
        if (manager->alarms[i].id == id) {
            manager->alarms[i].repeat = repeat;
            manager->alarms[i].days_of_week = days;
            return true;
        }
    }
    return false;
}

// 保存所有闹钟信息到NVS
void alarm_save_to_nvs(AlarmManager* manager) {
    if (!manager) return;
    
    // 创建JSON数组
    cJSON *alarmArray = cJSON_CreateArray();
    
    // 添加所有闹钟到数组
    for (int i = 0; i < manager->count; i++) {
        cJSON *alarmObj = cJSON_CreateObject();
        cJSON_AddNumberToObject(alarmObj, "id", manager->alarms[i].id);
        cJSON_AddStringToObject(alarmObj, "time", manager->alarms[i].time);
        cJSON_AddStringToObject(alarmObj, "message", manager->alarms[i].message);
        cJSON_AddBoolToObject(alarmObj, "enabled", manager->alarms[i].enabled);
        cJSON_AddBoolToObject(alarmObj, "repeat", manager->alarms[i].repeat);
        cJSON_AddNumberToObject(alarmObj, "days_of_week", manager->alarms[i].days_of_week);
        cJSON_AddItemToArray(alarmArray, alarmObj);
    }
    
    // 创建根对象
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "Alarm", alarmArray);
    
    // 转换为字符串并保存到NVS
    char *jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
        Settings settings("AlarmManager", true);
        settings.SetString("AlarmInfo", jsonStr);
        settings.SetInt("count", manager->count);
        // settings.SetInt("capacity", manager->capacity);//default is 10
        settings.SetInt("next_id", manager->next_id);
        settings.SetBool("enable", true);
        free(jsonStr);
    }
    
    cJSON_Delete(root);
}

// 外部调用接口函数
bool alarm_modify_callback(int id, const char* time, const char* message, bool enabled, bool repeat, int days) {
    if(alarm_mgr == NULL) {
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
        return false;
    } else {
        bool ret = alarm_modify(alarm_mgr, id, time, message, enabled, repeat, days);
        if(ret) {
            alarm_save_to_nvs(alarm_mgr); // 保存到NVS
            alarm_display_all(alarm_mgr);
        }
        return ret;
    }
}

bool alarm_enable_callback(int id, bool enable) {
    if(alarm_mgr == NULL) {
        ESP_LOGI(TAG, "\n请先初始化闹钟管理器\n");
        return false;
    } else {
        bool ret = alarm_enable(alarm_mgr, id, enable);
        if(ret) {
            alarm_save_to_nvs(alarm_mgr); // 保存到NVS
            alarm_display_all(alarm_mgr);
        }
        return ret;
    }
}