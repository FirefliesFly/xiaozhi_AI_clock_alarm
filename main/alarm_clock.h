#ifndef _ALARM_CLOCK_H_
#define _ALARM_CLOCK_H_ 

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void alarm_clock_test(void);
int alarm_clock_init(void);
void alarm_clock_check(void);
bool alarm_create_callback(const char* time, const char* message, bool repeat, int days);
bool alarm_delete_callback(int id);
uint8_t get_alarm_counts(void);

typedef void (*alarm_clock_ring_func_t)(void);
extern alarm_clock_ring_func_t alarm_clock_ring;

#ifdef __cplusplus
}
#endif
#endif