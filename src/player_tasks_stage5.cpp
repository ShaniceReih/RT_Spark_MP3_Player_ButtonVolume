#include "player_tasks.h"

#if defined(RTOS_STAGE5_PLAYER) && RTOS_STAGE5_PLAYER

#include "rtos_support.h"
#include "FreeRTOS.h"
#include "task.h"
#include "audio_codec.h"
#include "codec_control.h"
#include "player_state.h"
#include "player_presentation.h"
#include "lcd_control.h"

#include <cstdio>
#include <cstring>

extern Song *songs[8]; // Existing immutable-lifetime selector table; no score copies.

extern "C" void PollButtonsTask(void *argument);
extern "C" void AdjustVolumeTask(void *argument);
extern "C" void UpdateLcdLedsTask(void *argument);

namespace {
UART_HandleTypeDef *playerUart = nullptr;
bool playerCodecInitialized = false;
void (*completePlayerLoop)(bool) = nullptr;
TaskHandle_t pollButtonsHandle = nullptr;
TaskHandle_t adjustVolumeHandle = nullptr;
TaskHandle_t updateLcdLedsHandle = nullptr;
volatile uint32_t volumeWakeCount = 0;
volatile uint32_t lcdWakeCount = 0;
volatile uint32_t uiStateRevision = 0;
volatile uint32_t uiVolumeRevision = 0;
VolumeTaskDiagnostics volumeDiagnostics;

uint32_t lastHealthReport = 0;
constexpr uint32_t healthIntervalMs = 30000;

void uartWrite(const char *text)
{
    if (HAL_UART_Transmit(playerUart,
                          reinterpret_cast<uint8_t *>(const_cast<char *>(text)),
                          static_cast<uint16_t>(std::strlen(text)), 1000U) != HAL_OK)
        RtosSupport_Fatal("STAGE 5 UART TRANSMIT FAILED");
}
} // namespace

extern "C" void PollButtonsTask(void *)
{
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    uartWrite("POLL BUTTONS TASK STARTED\r\nSCHEDULER RUNNING\r\n");

    // A transmission finishes before handing UART ownership to the next task.
    // No other task prints after this one-time startup chain completes.
    xTaskNotifyGive(adjustVolumeHandle);
    configASSERT(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0U);

    configASSERT(NVIC_GetPriority(SysTick_IRQn) == 15U);
    configASSERT(NVIC_GetPriority(PendSV_IRQn) == 15U);
    if (playerCodecInitialized)
        configASSERT(NVIC_GetPriority(DMA1_Stream5_IRQn) == 5U);

    lastHealthReport = HAL_GetTick();
    completePlayerLoop(playerCodecInitialized);
    RtosSupport_Fatal("STAGE 5 PLAYER LOOP RETURNED");
}

extern "C" void AdjustVolumeTask(void *)
{
    configASSERT(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0U);
    uartWrite("ADJUST VOLUME TASK STARTED\r\n");
    PlayerState_BindVolumePublisher();
    uartWrite("STAGE 4: VOLUME TASK ACTIVE\r\n");
    xTaskNotifyGive(updateLcdLedsHandle);

    // Startup already seeded this same module. Never reset its filter here.
    for (;;) {
        const PotentiometerResult result = PotentiometerVolume_Update(HAL_GetTick());
        const PotentiometerReading reading = PotentiometerVolume_GetReading();
        if (result == PotentiometerResult::VolumeChanged && playerCodecInitialized) {
            CodecControl_Lock();
            AudioCodec_SetVolume(reading.volume);
            CodecControl_Unlock();
        }
        const uint32_t acceptedAt = HAL_GetTick();
        if (result == PotentiometerResult::Sampled ||
            result == PotentiometerResult::VolumeChanged)
            PlayerState_PublishVolume(reading.volume, reading.valid, acceptedAt);

        // Copy-only diagnostics leave runtime UART/newlib with PollButtonsTask.
        // BASEPRI critical sections do not mask the priority-5 audio interrupt.
        if (result == PotentiometerResult::VolumeChanged) {
            taskENTER_CRITICAL();
            volumeDiagnostics.changedReading = reading;
            if (++volumeDiagnostics.changedRevision == 0)
                ++volumeDiagnostics.changedRevision;
            taskEXIT_CRITICAL();
        }
        else if (result == PotentiometerResult::InitializationFailed ||
                 result == PotentiometerResult::ConversionFailed) {
            taskENTER_CRITICAL();
            volumeDiagnostics.error = result;
            volumeDiagnostics.errorAt = acceptedAt;
            if (++volumeDiagnostics.errorRevision == 0) ++volumeDiagnostics.errorRevision;
            taskEXIT_CRITICAL();
        }
        ++volumeWakeCount; // Counts real acquisition/update iterations now.
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void UpdateLcdLedsTask(void *)
{
    configASSERT(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0U);
    uartWrite("UPDATE LCD/LEDS TASK STARTED\r\n");
    LcdControl_BindOwner();
    PlayerPresentationObserver presentation(songs, 8);
    uartWrite("STAGE 5: LCD/LED TASK ACTIVE\r\n");
    xTaskNotifyGive(pollButtonsHandle);

    const TickType_t period = pdMS_TO_TICKS(20);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        PlayerStateSnapshot snapshot;
        PlayerState_GetSnapshot(snapshot); // The state mutex is released here.
        const uint32_t now = HAL_GetTick();
        const PlayerPresentationFrame frame = presentation.Observe(snapshot, now);

        // Only synchronous UI/cache/drawing work is inside the LCD mutex.
        LcdControl_Lock();
        for (uint8_t i = 0; i < frame.actionCount; ++i) {
            const PlayerPresentationAction &action = frame.actions[i];
            switch (action.kind) {
            case PlayerPresentationActionKind::CancelTransient:
                PlayerUI_CancelTransient();
                break;
            case PlayerPresentationActionKind::ConfirmationReady:
                PlayerUI_ConfirmationReady(action.timestamp);
                break;
            case PlayerPresentationActionKind::VolumeChanged:
                PlayerUI_VolumeChanged(action.timestamp);
                break;
            }
        }
        PlayerUI_Update(frame.view, now);
        LcdControl_Unlock();
        PlayerLeds_SetState(frame.led); // GPIO needs no LCD mutex.
        uiStateRevision = frame.stateRevision;
        uiVolumeRevision = frame.volumeRevision;
        ++lcdWakeCount;

        // Keep a stable quiet-frame cadence without spinning after a long draw.
        const TickType_t finished = xTaskGetTickCount();
        if (finished - wake >= period) wake = finished;
        vTaskDelayUntil(&wake, period);
    }
}

void PlayerTasks_Create(UART_HandleTypeDef *uart, bool codecInitialized,
                        void (*runCompletePlayerLoop)(bool))
{
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    configASSERT(uart != nullptr && runCompletePlayerLoop != nullptr);
    configASSERT(pollButtonsHandle == nullptr && adjustVolumeHandle == nullptr &&
                 updateLcdLedsHandle == nullptr);

    // The Cortex-M port resets MSP at scheduler startup. Keep bootstrap values
    // in static storage rather than passing pointers into main's former stack.
    playerUart = uart;
    playerCodecInitialized = codecInitialized;
    completePlayerLoop = runCompletePlayerLoop;

    configASSERT(xTaskCreate(PollButtonsTask, "PollButtons", 1024U, nullptr,
                             3U, &pollButtonsHandle) == pdPASS);
    configASSERT(xTaskCreate(AdjustVolumeTask, "AdjustVolume", 768U, nullptr,
                             2U, &adjustVolumeHandle) == pdPASS);
    configASSERT(xTaskCreate(UpdateLcdLedsTask, "UpdateLcdLeds", 1024U, nullptr,
                             1U, &updateLcdLedsHandle) == pdPASS);

    uartWrite("\r\nFREERTOS PLAYER MODE\r\n");
    char message[128];
    std::snprintf(message, sizeof(message), "SystemCoreClock: %lu Hz\r\n",
                  static_cast<unsigned long>(SystemCoreClock));
    uartWrite(message);
    uartWrite("TASKS CREATED: BUTTONS=3/1024w VOLUME=2/768w LCD/LEDS=1/1024w\r\n");
}

void PlayerTasks_StartScheduler()
{
    configASSERT(pollButtonsHandle != nullptr && adjustVolumeHandle != nullptr &&
                 updateLcdLedsHandle != nullptr);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    uartWrite("Scheduler starting...\r\n");
    vTaskStartScheduler();
    RtosSupport_Fatal("STAGE 5 SCHEDULER RETURNED");
}

void PlayerTasks_ReportHealth(uint32_t now)
{
    configASSERT(xTaskGetCurrentTaskHandle() == pollButtonsHandle);
    if (now - lastHealthReport < healthIntervalMs) return;
    lastHealthReport = now;

    const UBaseType_t buttonsStack = uxTaskGetStackHighWaterMark(pollButtonsHandle);
    const UBaseType_t volumeStack = uxTaskGetStackHighWaterMark(adjustVolumeHandle);
    const UBaseType_t lcdStack = uxTaskGetStackHighWaterMark(updateLcdLedsHandle);
    const uint32_t volumeWakes = volumeWakeCount;
    const uint32_t lcdWakes = lcdWakeCount;
    const size_t heap = xPortGetFreeHeapSize();
    const size_t minimumHeap = xPortGetMinimumEverFreeHeapSize();

    char message[256];
    std::snprintf(message, sizeof(message),
                  "RTOS HEALTH: BUTTON_STACK=%luw VOLUME_STACK=%luw "
                  "LCD_STACK=%luw VOLUME_WAKES=%lu LCD_WAKES=%lu "
                  "HEAP=%lu MIN_HEAP=%lu UI_STATE_REV=%lu UI_VOL_REV=%lu\r\n",
                  static_cast<unsigned long>(buttonsStack),
                  static_cast<unsigned long>(volumeStack),
                  static_cast<unsigned long>(lcdStack),
                  static_cast<unsigned long>(volumeWakes),
                  static_cast<unsigned long>(lcdWakes),
                  static_cast<unsigned long>(heap),
                  static_cast<unsigned long>(minimumHeap),
                  static_cast<unsigned long>(uiStateRevision),
                  static_cast<unsigned long>(uiVolumeRevision));
    uartWrite(message);
}

void PlayerTasks_GetVolumeDiagnostics(VolumeTaskDiagnostics &diagnostics)
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetCurrentTaskHandle() == pollButtonsHandle);
    taskENTER_CRITICAL();
    diagnostics = volumeDiagnostics;
    taskEXIT_CRITICAL();
}

#endif // RTOS_STAGE5_PLAYER
