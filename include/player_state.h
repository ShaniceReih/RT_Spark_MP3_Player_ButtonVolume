#ifndef RT_SPARK_PLAYER_STATE_H
#define RT_SPARK_PLAYER_STATE_H

#include <cstdint>

// Public state only. The selection controller remains private to its owner.
enum class PlayerMode : uint8_t { Idle, Selecting, Playing, Paused };
enum class PlayerPlayback : uint8_t { Idle, Playing, Paused, Finished, Error };
enum class PlayerSelectionPhase : uint8_t { None, Release, Confirm };
enum class PlayerEventKind : uint8_t
{
    Captured,
    ConfirmationReady,
    Timeout,
    Confirmed,
    Paused,
    Resumed,
    Completed,
    PlaybackError,
    PresentationCancelled,
    VolumeChanged,
    Count
};

struct PlayerEventStamp
{
    uint32_t revision = 0; // Zero means this event has never occurred.
    uint32_t timestamp = 0;
    bool overlayEligible = false; // Meaningful for VolumeChanged only.
};

struct PlayerStateSnapshot
{
    uint32_t revision = 0; // Assigned by the shared store on publication.
    uint32_t observedAt = 0;
    PlayerMode mode = PlayerMode::Idle;
    PlayerMode background = PlayerMode::Idle;
    uint8_t currentSong = 0;
    uint8_t pendingSong = 0;
    uint8_t previewBits = 0;
    uint8_t capturedBits = 0;
    bool hasCurrentSong = false;
    bool acceptedVolumeValid = false;
    PlayerSelectionPhase phase = PlayerSelectionPhase::None;
    // Both confirmation fields are meaningful only while phase == Confirm.
    uint32_t confirmationStart = 0;
    uint32_t confirmationDeadline = 0;
    bool timeoutVisible = false;
    uint32_t timeoutStart = 0;
    uint8_t acceptedVolume = 0;
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
    // Assigned only when the volume owner accepts a different logical value.
    uint32_t volumeRevision = 0;
#endif
    PlayerPlayback playback = PlayerPlayback::Idle;
    // Retained outcome; Idle means no completion/error has been observed yet.
    PlayerPlayback lastCompletion = PlayerPlayback::Idle;
    uint32_t eventSequence = 0;
    PlayerEventStamp events[static_cast<uint8_t>(PlayerEventKind::Count)] = {};
};

// Record on the owner's private snapshot. Last occurrence per kind persists;
// repeated occurrences of the same kind coalesce instead of forming a queue.
inline void PlayerState_RecordEvent(PlayerStateSnapshot &snapshot,
                                   PlayerEventKind kind, uint32_t now,
                                   bool overlayEligible = false)
{
    const uint8_t index = static_cast<uint8_t>(kind);
    if (index >= static_cast<uint8_t>(PlayerEventKind::Count)) return;
    if (++snapshot.eventSequence == 0) ++snapshot.eventSequence;
    PlayerEventStamp &stamp = snapshot.events[index];
    stamp.revision = snapshot.eventSequence;
    stamp.timestamp = now;
    stamp.overlayEligible = kind == PlayerEventKind::VolumeChanged &&
                            overlayEligible;
}

// Wrap-safe order for revisions less than half of the uint32 range apart.
// Event revision zero is the absent-event sentinel, never a recorded event.
inline bool PlayerState_EventAfter(uint32_t candidate, uint32_t reference)
{
    if (candidate == 0 || candidate == reference) return false;
    if (reference == 0) return true;
    return candidate - reference < 0x80000000U;
}

#if (defined(RTOS_STAGE3_PLAYER) && RTOS_STAGE3_PLAYER) || (defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER)
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
// Init runs before scheduling. Control and volume owners then bind separately.
#else
// Init runs before scheduling; the sole publisher binds in PollButtonsTask.
#endif
void PlayerState_Init(uint8_t acceptedVolume, bool valid);
void PlayerState_BindPublisher();
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
// Merge control fields/events only; volume fields retain the volume owner's
// publication. Both producers receive one globally ordered shared event clock.
#else
// Whole-copy publication is single-owner only. Before Stage 4 adds a volume
// writer, replace it with atomic field merges; never publish stale volume data.
#endif
void PlayerState_Publish(const PlayerStateSnapshot &snapshot);
// True means the blocking copy succeeded, not that its revision is new.
bool PlayerState_GetSnapshot(PlayerStateSnapshot &snapshot);
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
// The independent volume owner binds once after scheduler startup. Failed ADC
// samples are omitted; an invalid publication never replaces accepted volume.
void PlayerState_BindVolumePublisher();
void PlayerState_PublishVolume(uint8_t acceptedVolume, bool valid,
                               uint32_t acceptedAt);
// Control-owner presentation acknowledgement after an eligible volume event.
// This changes neither accepted volume nor retained event revisions.
void PlayerState_DismissTimeoutNotice();
#endif
#endif

#endif
