#include "player_state.h"

#if defined(RTOS_STAGE3_PLAYER) && RTOS_STAGE3_PLAYER && !(defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER)

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

namespace {
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t publisher = nullptr;
PlayerStateSnapshot sharedState;

void assertTaskContext()
{
    // An IRQ may not wait for a task mutex, even if it uses no FromISR API.
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
}
} // namespace

void PlayerState_Init(uint8_t acceptedVolume, bool valid)
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    configASSERT(stateMutex == nullptr);
    sharedState = PlayerStateSnapshot{};
    sharedState.acceptedVolume = acceptedVolume;
    sharedState.acceptedVolumeValid = valid;
    stateMutex = xSemaphoreCreateMutex();
    configASSERT(stateMutex != nullptr);
}

void PlayerState_BindPublisher()
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && publisher == nullptr);
    publisher = xTaskGetCurrentTaskHandle();
    configASSERT(publisher != nullptr);
}

void PlayerState_Publish(const PlayerStateSnapshot &snapshot)
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && publisher != nullptr);
    configASSERT(xTaskGetCurrentTaskHandle() == publisher);
    configASSERT(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE);
    uint32_t nextRevision = sharedState.revision + 1U;
    if (nextRevision == 0U) ++nextRevision;
    sharedState = snapshot;
    sharedState.revision = nextRevision;
    configASSERT(xSemaphoreGive(stateMutex) == pdTRUE);
}

bool PlayerState_GetSnapshot(PlayerStateSnapshot &snapshot)
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr);
    configASSERT(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE);
    snapshot = sharedState;
    configASSERT(xSemaphoreGive(stateMutex) == pdTRUE);
    return true;
}

#endif
