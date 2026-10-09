#include "player_state.h"

#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

namespace {
constexpr uint8_t EventCount = static_cast<uint8_t>(PlayerEventKind::Count);
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t controlPublisher = nullptr;
TaskHandle_t volumePublisher = nullptr;
PlayerStateSnapshot sharedState;
uint32_t controlEventSeen[EventCount] = {};

void assertTaskContext()
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
}

uint32_t nextRevision(uint32_t revision)
{
    if (++revision == 0U) ++revision;
    return revision;
}

uint8_t boundedVolume(uint8_t volume)
{
    return volume > 100U ? 100U : volume;
}
} // namespace

void PlayerState_Init(uint8_t acceptedVolume, bool valid)
{
    configASSERT(__get_IPSR() == 0U);
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);
    configASSERT(stateMutex == nullptr);
    sharedState = PlayerStateSnapshot{};
    sharedState.acceptedVolume = boundedVolume(acceptedVolume);
    sharedState.acceptedVolumeValid = valid;
    stateMutex = xSemaphoreCreateMutex();
    configASSERT(stateMutex != nullptr);
}

void PlayerState_BindPublisher()
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && controlPublisher == nullptr);
    const TaskHandle_t owner = xTaskGetCurrentTaskHandle();
    configASSERT(owner != nullptr && owner != volumePublisher);
    controlPublisher = owner;
}

void PlayerState_BindVolumePublisher()
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && volumePublisher == nullptr);
    const TaskHandle_t owner = xTaskGetCurrentTaskHandle();
    configASSERT(owner != nullptr && owner != controlPublisher);
    volumePublisher = owner;
}

void PlayerState_Publish(const PlayerStateSnapshot &snapshot)
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && controlPublisher != nullptr);
    configASSERT(xTaskGetCurrentTaskHandle() == controlPublisher);

    // Private control event numbers are independent of the shared event clock.
    // Collect and order only newly observed control events before taking the
    // mutex, so multiple events in one controller update keep their ordering.
    uint8_t pending[EventCount] = {};
    uint8_t count = 0;
    for (uint8_t kind = 0; kind < EventCount; ++kind) {
        if (kind == static_cast<uint8_t>(PlayerEventKind::VolumeChanged)) continue;
        const uint32_t revision = snapshot.events[kind].revision;
        if (!PlayerState_EventAfter(revision, controlEventSeen[kind])) continue;
        uint8_t position = count;
        while (position > 0U && PlayerState_EventAfter(
                   snapshot.events[pending[position - 1U]].revision, revision)) {
            pending[position] = pending[position - 1U];
            --position;
        }
        pending[position] = kind;
        ++count;
    }

    configASSERT(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE);
    // Copy control-owned fields explicitly. In particular, never overwrite the
    // accepted volume, validity, volume revision or its retained event stamp.
    sharedState.observedAt = snapshot.observedAt;
    sharedState.mode = snapshot.mode;
    sharedState.background = snapshot.background;
    sharedState.currentSong = snapshot.currentSong;
    sharedState.pendingSong = snapshot.pendingSong;
    sharedState.previewBits = snapshot.previewBits;
    sharedState.capturedBits = snapshot.capturedBits;
    sharedState.hasCurrentSong = snapshot.hasCurrentSong;
    sharedState.phase = snapshot.phase;
    sharedState.confirmationStart = snapshot.confirmationStart;
    sharedState.confirmationDeadline = snapshot.confirmationDeadline;
    sharedState.timeoutVisible = snapshot.timeoutVisible;
    sharedState.timeoutStart = snapshot.timeoutStart;
    sharedState.playback = snapshot.playback;
    sharedState.lastCompletion = snapshot.lastCompletion;
    for (uint8_t item = 0; item < count; ++item) {
        const uint8_t kind = pending[item];
        const PlayerEventStamp &stamp = snapshot.events[kind];
        PlayerState_RecordEvent(sharedState, static_cast<PlayerEventKind>(kind),
                                stamp.timestamp);
        controlEventSeen[kind] = stamp.revision;
    }
    sharedState.revision = nextRevision(sharedState.revision);
    configASSERT(xSemaphoreGive(stateMutex) == pdTRUE);
}

void PlayerState_PublishVolume(uint8_t acceptedVolume, bool valid,
                               uint32_t acceptedAt)
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && volumePublisher != nullptr);
    configASSERT(xTaskGetCurrentTaskHandle() == volumePublisher);
    // The verified ADC module retains its previous accepted value on failure.
    // Defensively ignore invalid data rather than damaging the public value.
    if (!valid) return;
    const uint8_t bounded = boundedVolume(acceptedVolume);
    configASSERT(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE);
    const bool changed = bounded != sharedState.acceptedVolume;
    const bool becameValid = !sharedState.acceptedVolumeValid;
    if (changed || becameValid) {
        sharedState.acceptedVolume = bounded;
        sharedState.acceptedVolumeValid = true;
        if (changed) {
            sharedState.volumeRevision = nextRevision(sharedState.volumeRevision);
            const bool overlay = sharedState.mode == PlayerMode::Playing ||
                                 sharedState.mode == PlayerMode::Paused;
            PlayerState_RecordEvent(sharedState, PlayerEventKind::VolumeChanged,
                                    acceptedAt, overlay);
        }
        sharedState.revision = nextRevision(sharedState.revision);
    }
    configASSERT(xSemaphoreGive(stateMutex) == pdTRUE);
}

void PlayerState_DismissTimeoutNotice()
{
    assertTaskContext();
    configASSERT(stateMutex != nullptr && controlPublisher != nullptr);
    configASSERT(xTaskGetCurrentTaskHandle() == controlPublisher);
    configASSERT(xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE);
    if (sharedState.timeoutVisible) {
        sharedState.timeoutVisible = false;
        sharedState.revision = nextRevision(sharedState.revision);
    }
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
