#ifndef PLAYER_UI_H
#define PLAYER_UI_H

#include <cstdint>
#include "song.h"

// Presentation only. Audio and input remain owned by main/the existing drivers.
void PlayerUI_ShowStartup();
void PlayerUI_ShowAudioReady(uint32_t now);
void PlayerUI_ShowAudioFailure(uint32_t now);

enum class UiPlaybackState { Idle, Playing, Paused };
enum class UiSelectionState { None, Release, Confirm };

struct PlayerUiView
{
    UiPlaybackState playback = UiPlaybackState::Idle;
    const Song *currentSong = nullptr;
    uint8_t currentIndex = 0;
    // Supplied from the same main-loop value passed to AudioCodec_SetVolume.
    uint8_t volume = 0;
    UiSelectionState selection = UiSelectionState::None;
    const Song *candidateSong = nullptr;
    uint8_t candidateIndex = 0;
    bool timeoutVisible = false;
};

void PlayerUI_ConfirmationReady(uint32_t inputNow);
void PlayerUI_VolumeChanged(uint32_t now);
void PlayerUI_CancelTransient();
// One foreground rendering entry point; no audio/HAL/timer ownership.
void PlayerUI_Update(const PlayerUiView &view, uint32_t now);

#endif
