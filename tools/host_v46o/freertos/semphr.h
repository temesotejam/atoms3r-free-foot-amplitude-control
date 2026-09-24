#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t=void*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return reinterpret_cast<void*>(1);}
inline int xSemaphoreTake(SemaphoreHandle_t, unsigned){return pdTRUE;}
inline int xSemaphoreGive(SemaphoreHandle_t){return pdTRUE;}
