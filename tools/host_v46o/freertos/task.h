#pragma once
#include "FreeRTOS.h"
using TaskHandle_t = void*;
inline uint32_t host_tasks_created=0;
inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,uint32_t,void*,uint32_t,TaskHandle_t* h,uint32_t){++host_tasks_created;*h=reinterpret_cast<void*>(1);return pdPASS;}
inline void vTaskDelete(TaskHandle_t) {}
inline void xTaskNotifyGive(TaskHandle_t) {}
inline uint32_t ulTaskNotifyTake(BaseType_t,TickType_t) {return 1;}
inline void vTaskDelay(TickType_t n) { delay(n); }
inline int xPortGetCoreID() {return 1;}
inline UBaseType_t uxTaskPriorityGet(TaskHandle_t) {return 2;}
