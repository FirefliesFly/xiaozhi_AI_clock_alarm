#ifndef _ALARM_CLOCK_H_
#define _ALARM_CLOCK_H_ 

#include "alarm_clock_menu.h"
#include "alarm_clock_driver.h"
#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_MAX_COUNT 10

void alarm_clock_test(void);
int alarm_clock_init(void);
void alarm_clock_check(void);
bool alarm_create_callback(const char* time, const char* message, bool repeat, int days);
bool alarm_delete_callback(int id);
uint8_t get_alarm_counts(void);
bool CheckAlarmManagerFromNv(void);
void alarm_save_to_nvs(AlarmManager* manager);

typedef void (*alarm_clock_ring_func_t)(void);
extern alarm_clock_ring_func_t alarm_clock_ring;
extern AlarmManager* alarm_mgr;
#ifdef __cplusplus
}
#endif
#endif