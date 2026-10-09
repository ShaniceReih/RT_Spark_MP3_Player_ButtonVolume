#ifndef RT_SPARK_PLAYER_TASKS_H
#define RT_SPARK_PLAYER_TASKS_H

#if defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER

#include "stm32f4xx_hal.h"
#include <cstdint>

// Stage 2: one task owns the complete verified loop; the other two are shells.
void PlayerTasks_Create(UART_HandleTypeDef *uart, bool codecInitialized,
                        void (*runCompletePlayerLoop)(bool));
void PlayerTasks_StartScheduler() __attribute__((noreturn));
// Called only by the complete player loop, before its normal 20 ms task delay.
void PlayerTasks_ReportHealth(uint32_t now);

#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
#include "potentiometer_volume.h"
// Diagnostic copies only: no ADC/filter access and no presentation decisions.
struct VolumeTaskDiagnostics
{
    uint32_t changedRevision = 0;
    PotentiometerReading changedReading;
    uint32_t errorRevision = 0;
    PotentiometerResult error = PotentiometerResult::NotDue;
    uint32_t errorAt = 0;
};
void PlayerTasks_GetVolumeDiagnostics(VolumeTaskDiagnostics &diagnostics);
#endif

#endif

#endif
