#ifndef RT_SPARK_PLAYER_PRESENTATION_H
#define RT_SPARK_PLAYER_PRESENTATION_H

#include "player_state.h"
#include "player_ui.h"
#include "player_leds.h"

// Presentation decisions only: no HAL, RTOS, drawing or player-state mutation.
enum class PlayerPresentationScreen : uint8_t
{
    Idle, Release, Confirm, Playing, Paused, Timeout, Volume
};

enum class PlayerPresentationActionKind : uint8_t
{
    CancelTransient, ConfirmationReady, VolumeChanged
};

struct PlayerPresentationAction
{
    PlayerPresentationActionKind kind = PlayerPresentationActionKind::CancelTransient;
    uint32_t timestamp = 0;
    uint32_t revision = 0;
};

constexpr uint8_t PlayerPresentationEventCount =
    static_cast<uint8_t>(PlayerEventKind::Count);

struct PlayerPresentationFrame
{
    PlayerUiView view;
    PlayerLedState led = PLAYER_LED_IDLE;
    PlayerPresentationScreen screen = PlayerPresentationScreen::Idle;
    PlayerPresentationAction actions[PlayerPresentationEventCount];
    uint8_t actionCount = 0;
    uint8_t confirmationSeconds = 0; // Display only; never determines expiry.
    uint32_t stateRevision = 0;
    uint32_t volumeRevision = 0;
};

class PlayerPresentationObserver
{
public:
    PlayerPresentationObserver(Song *const *songs, uint8_t songCount);
    PlayerPresentationFrame Observe(const PlayerStateSnapshot &snapshot,
                                    uint32_t now);
    uint32_t LastSeen(PlayerEventKind kind) const;

private:
    const Song *songAt(uint8_t index) const;
    Song *const *songs_;
    uint8_t songCount_;
    uint32_t seen_[PlayerPresentationEventCount] = {};
    uint32_t stateRevision_ = 0;
    uint32_t volumeRevision_ = 0;
    bool overlayActive_ = false;
    uint32_t overlayStart_ = 0;
    bool confirmationSeen_ = false;
    uint32_t confirmationStart_ = 0;
    PlayerPresentationScreen screen_ = PlayerPresentationScreen::Idle;
};

#endif
