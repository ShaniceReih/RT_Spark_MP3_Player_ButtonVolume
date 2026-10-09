#include "lcd_control.h"

#if defined(RTOS_STAGE5_PLAYER) && RTOS_STAGE5_PLAYER
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

namespace {
SemaphoreHandle_t lcdMutex = nullptr;
TaskHandle_t lcdOwner = nullptr;

void assertOwner()
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(lcdMutex != nullptr && lcdOwner != nullptr);
    configASSERT(xTaskGetCurrentTaskHandle() == lcdOwner);
}
}

void LcdControl_Init()
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    configASSERT(lcdMutex == nullptr);
    lcdMutex = xSemaphoreCreateMutex();
    configASSERT(lcdMutex != nullptr);
}

void LcdControl_BindOwner()
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(lcdMutex != nullptr && lcdOwner == nullptr);
    lcdOwner = xTaskGetCurrentTaskHandle();
    configASSERT(lcdOwner != nullptr);
}

void LcdControl_Lock()
{
    assertOwner();
    configASSERT(xSemaphoreTake(lcdMutex, portMAX_DELAY) == pdTRUE);
}

void LcdControl_Unlock()
{
    assertOwner();
    configASSERT(xSemaphoreGive(lcdMutex) == pdTRUE);
}
#endif
