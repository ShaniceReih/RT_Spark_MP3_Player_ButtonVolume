#include "player_ui.h"
#include "lcd_display.h"
#include "song_selection.h"
#include "song_def.h" // Actual metadata is defined in this translation unit only.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class DrawKind { FullFill, Rectangle, Text };
struct Draw {
    DrawKind kind;
    uint16_t x, y, width, height, color;
    uint8_t scale;
    std::string text;
};

std::vector<Draw> calls;
std::vector<Draw> visibleText;

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

bool intersects(const Draw &a, const Draw &b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

void eraseText(const Draw &region) {
    visibleText.erase(std::remove_if(visibleText.begin(), visibleText.end(),
        [&](const Draw &text) { return intersects(text, region); }), visibleText.end());
}

bool hasText(const std::string &text) {
    return std::any_of(visibleText.begin(), visibleText.end(),
        [&](const Draw &draw) { return draw.text == text; });
}

const Draw &textAt(uint16_t y) {
    const auto found = std::find_if(visibleText.begin(), visibleText.end(),
        [&](const Draw &draw) { return draw.y == y && !draw.text.empty(); });
    require(found != visibleText.end(), "Expected text row is missing");
    return *found;
}

unsigned fullFills() {
    return static_cast<unsigned>(std::count_if(calls.begin(), calls.end(),
        [](const Draw &draw) { return draw.kind == DrawKind::FullFill; }));
}

void expectPartial(const char *message) {
    require(!calls.empty(), "Expected a visible change");
    require(fullFills() == 0, message);
}

void expectCountdownOnly() {
    expectPartial("Countdown cleared the whole confirmation screen");
    for (const Draw &draw : calls) {
        require(draw.y >= 228 && draw.y + draw.height <= 237,
                "Countdown redrew unrelated screen content");
    }
}

void freshUi() {
    PlayerUI_CancelTransient();
    PlayerUI_ShowStartup(); // Force a known screen boundary without private state access.
    PlayerUI_CancelTransient();
    calls.clear();
}

const Song *songs[] = {
    &FUR_ELISE, &CANNON_IN_D, &MINUET_IN_G_MAJOR, &TURKISH_MARCH,
    &NOCTRUNE_IN_E_FLAT, &WALTZ_NO2, &NOCTRUNE_IN_C_SHARP_MAJOR, &SYMPHONY_NO40
};

PlayerUiView playback(uint8_t index = 1, uint8_t volume = 50,
                      UiPlaybackState state = UiPlaybackState::Playing) {
    PlayerUiView view;
    view.playback = state;
    view.currentSong = songs[index];
    view.currentIndex = index;
    view.candidateSong = songs[0];
    view.volume = volume;
    return view;
}

PlayerUiView candidate(uint8_t index, UiSelectionState state = UiSelectionState::None) {
    PlayerUiView view;
    view.candidateSong = songs[index];
    view.candidateIndex = index;
    view.selection = state;
    view.volume = 50;
    return view;
}

void expectBar(uint16_t interiorY, uint8_t volume, uint16_t color) {
    const uint16_t expected = 196U * volume / 100;
    unsigned fills = 0;
    bool cleared = false;
    for (const Draw &draw : calls) {
        if (draw.kind == DrawKind::Rectangle && draw.x == 22 &&
            draw.y == interiorY && draw.width == 196 && draw.height == 12 &&
            draw.color == COLOR_BLACK) cleared = true;
        if (draw.kind != DrawKind::Rectangle || draw.x != 22 ||
            draw.y != interiorY || draw.height != 12 || draw.color != color) continue;
        require(cleared, "Volume fill did not clear the old interior first");
        ++fills;
        require(draw.width == expected, "Volume bar does not match codec-setting percentage");
    }
    require(cleared, "Volume update did not erase the old bar interior");
    require(fills == (volume == 0 ? 0U : 1U), "Missing or duplicate volume fill");
}

void startupStatus() {
    freshUi();
    PlayerUI_ShowStartup();
    require(hasText("RT-SPARK") && hasText("MP3 PLAYER") && hasText("INITIALIZING"),
            "Startup branding/status missing");
    const auto view = candidate(0);
    PlayerUI_ShowAudioReady(100);
    require(hasText("AUDIO READY"), "Ready status missing");
    calls.clear();
    PlayerUI_Update(view, 699);
    require(calls.empty() && hasText("AUDIO READY"), "Ready splash ended before 600 ms");
    PlayerUI_Update(view, 700);
    require(hasText("SELECT SONG"), "Ready splash did not yield to idle");

    const uint32_t start = UINT32_MAX - 300U;
    PlayerUI_ShowAudioFailure(start);
    require(hasText("INIT FAILED"), "Detected audio failure status missing");
    calls.clear();
    PlayerUI_Update(view, start + 599U);
    require(calls.empty(), "Failure splash timing broke across tick wrap");
    PlayerUI_Update(view, start + 600U);
    require(hasText("SELECT SONG"), "Failure splash did not yield across tick wrap");
}

void realSongMetadata() {
    const char *first[] = {"FUR ELISE", "CANON IN D", "MINUET IN", "TURKISH",
                          "NOCTURNE", "WALTZ NO 2", "NOCTURNE", "SYMPHONY"};
    const char *second[] = {"", "", "G MAJOR", "MARCH", "IN E FLAT", "",
                           "IN C SHARP", "NO 40"};
    const char *composer[] = {"BEETHOVEN", "PACHEBELBEL", "BACH", "MOZART", "CHOPIN",
                             "SHOSTAKOVICH", "CHOPIN", "MOZART"};
    for (uint8_t index = 0; index < 8; ++index) {
        freshUi();
        auto view = playback(index);
        PlayerUI_Update(view, 100);
        require(textAt(53).text == first[index], "Real song title's first line changed");
        require(textAt(53).scale == 3, "Current title needlessly shrank below scale 3");
        if (*second[index]) {
            require(textAt(81).text == second[index], "Real song title's second line changed");
            require(textAt(81).scale == 3, "Wrapped current title shrank unexpectedly");
        }
        require(textAt(113).text == composer[index], "Source composer spelling changed");
        char expected[24];
        std::snprintf(expected, sizeof(expected), "SONG %u - %u%u%u", index + 1,
                      (index >> 2) & 1, (index >> 1) & 1, index & 1);
        require(hasText(expected), "Playing footer's song number/binary index disagree");
        calls.clear();
        PlayerUI_Update(view, 1000);
        require(calls.empty(), "Unchanged playing snapshot redrew the display");

        freshUi();
        PlayerUI_Update(candidate(index), 100);
        require(textAt(115).text == first[index], "Idle preview title differs from playback");
        require(textAt(177).text == composer[index], "Idle preview composer differs");
        for (uint8_t bit = 0; bit < 3; ++bit) {
            const uint16_t x = 61 + 50 * bit;
            const auto found = std::find_if(visibleText.begin(), visibleText.end(),
                [&](const Draw &draw) { return draw.x == x && draw.y == 46; });
            require(found != visibleText.end(), "Binary box digit is missing");
            require(found->text == ((index & (4 >> bit)) ? "1" : "0"),
                    "DOWN LEFT RIGHT box order is incorrect");
        }
    }
}

void boundedFutureMetadata() {
    Song longTitle("Extraordinarilylongwordabcdefghijklmnopqrstuvwxyz0123456789",
                   "evenmoreletters - Composer with unsupported %!? punctuation", nullptr,
                   nullptr, 0.1f, 0);
    freshUi();
    auto view = playback();
    view.currentSong = &longTitle;
    PlayerUI_Update(view, 100);
    require(textAt(53).scale < 3, "Long future title did not use font fallback");
    require(!textAt(113).text.empty(), "Long future composer disappeared");
    // Every LCD_DrawText call also checks bounds and the actual font alphabet.
}

void unchangedAndPartialRegions() {
    freshUi();
    auto idle = candidate(0);
    PlayerUI_Update(idle, 0);
    calls.clear();
    PlayerUI_Update(idle, 100);
    require(calls.empty(), "Unchanged idle preview redrew");
    idle = candidate(7);
    PlayerUI_Update(idle, 110);
    expectPartial("Changing idle preview cleared the whole screen");
    require(hasText("SYMPHONY") && hasText("NO 40") && hasText("MOZART"),
            "Changed preview did not show new metadata");
    require(!hasText("FUR ELISE") && !hasText("BEETHOVEN"), "Old preview text survived");

    freshUi();
    auto view = playback();
    PlayerUI_Update(view, 0);
    calls.clear();
    view.playback = UiPlaybackState::Paused;
    PlayerUI_Update(view, 100);
    expectPartial("Pause cleared the whole playback layout");
    require(hasText("PAUSED") && hasText("HOLD UP RESUME"), "Pause labels missing");
    require(!hasText("NOW PLAYING") && !hasText("PLAY") && !hasText("HOLD UP PAUSE"),
            "Playing labels survived pause");
    calls.clear();
    PlayerUI_Update(view, 101);
    require(calls.empty(), "Unchanged paused snapshot redrew");
    view.playback = UiPlaybackState::Playing;
    PlayerUI_Update(view, 200);
    expectPartial("Resume cleared the whole playback layout");
    require(hasText("NOW PLAYING") && hasText("PLAY") && hasText("HOLD UP PAUSE"),
            "Resume labels missing");
    require(!hasText("PAUSED") && !hasText("HOLD UP RESUME"), "Pause labels survived resume");
}

SelectionButtons chord(uint8_t bits, bool up) {
    SelectionButtons buttons;
    buttons.up = up;
    buttons.down = (bits & 4) != 0;
    buttons.left = (bits & 2) != 0;
    buttons.right = (bits & 1) != 0;
    return buttons;
}

void capturedSelectionAndDeadline() {
    freshUi();
    SongSelectionInput controller;
    auto view = playback();
    const auto sample = [&](uint32_t now, SelectionButtons raw) {
        const auto events = controller.update(now, raw, true);
        if (events.readyToConfirm) PlayerUI_ConfirmationReady(now);
        view.selection = controller.selecting()
            ? (controller.waitingForRelease() ? UiSelectionState::Release : UiSelectionState::Confirm)
            : UiSelectionState::None;
        if (controller.selecting()) {
            view.candidateIndex = controller.pendingSong();
            view.candidateSong = songs[view.candidateIndex];
        }
        if (events.timedOut) view.timeoutVisible = true;
        PlayerUI_Update(view, now);
        return events;
    };
    sample(0, {});
    sample(10, chord(3, false));
    sample(35, chord(3, false));
    sample(40, chord(3, true));
    require(sample(65, chord(3, true)).captured, "Controller did not capture selection");
    require(hasText("RELEASE ALL") && hasText("THEN UP CONFIRM"), "Release gate instructions missing");
    require(view.candidateIndex == 3 && hasText("TURKISH") && hasText("MARCH"),
            "Captured candidate is incorrect");
    calls.clear();
    sample(6000, chord(3, true));
    require(calls.empty(), "Holding the captured gesture started a UI countdown");
    sample(6010, chord(5, false));
    sample(6035, chord(5, false));
    require(controller.waitingForRelease(), "Release gate ignored still-held bit buttons");
    require(view.candidateIndex == 3 && hasText("MARCH"), "Raw bit change replaced frozen candidate");
    sample(6100, {});
    require(sample(6125, {}).readyToConfirm, "Controller did not start confirmation after all release");
    require(hasText("5 SEC") && hasText("UP CONFIRM"), "Confirmation did not start at five");
    require(!hasText("RELEASE ALL"), "Release instructions survived confirmation");
    calls.clear();
    sample(7124, {});
    require(calls.empty(), "Countdown changed before its one-second boundary");
    for (uint32_t second = 1; second <= 4; ++second) {
        calls.clear();
        sample(6125 + second * 1000, {});
        expectCountdownOnly();
        require(hasText(std::to_string(5 - second) + " SEC"), "Countdown missed exact boundary");
    }
    calls.clear();
    sample(11124, {});
    require(calls.empty() && hasText("1 SEC"), "Countdown expired before controller deadline");
    require(sample(11125, {}).timedOut, "Controller did not own five-second expiry");
    require(hasText("CANCELLED"), "Controller timeout notice missing");
    calls.clear();
    PlayerUI_Update(view, 999999);
    require(calls.empty() && hasText("CANCELLED"), "UI independently dismissed timeout notice");
    view.timeoutVisible = false;
    PlayerUI_Update(view, 1000000);
    require(hasText("NOW PLAYING"), "Timeout did not restore current playing state");
}

void countdownWrapAndControllerOwnership() {
    freshUi();
    auto view = candidate(7, UiSelectionState::Confirm);
    const uint32_t start = UINT32_MAX - 1500U;
    PlayerUI_ConfirmationReady(start);
    PlayerUI_Update(view, start);
    require(hasText("5 SEC"), "Wrapped confirmation did not start at five");
    for (uint32_t second = 1; second <= 4; ++second) {
        calls.clear();
        PlayerUI_Update(view, start + second * 1000U);
        expectCountdownOnly();
        require(hasText(std::to_string(5 - second) + " SEC"), "Countdown broke across tick wrap");
    }
    calls.clear();
    PlayerUI_Update(view, start + 5000U);
    require(calls.empty() && hasText("UP CONFIRM") && hasText("1 SEC"),
            "UI invented expiry without a controller timeout snapshot");
    view.selection = UiSelectionState::None;
    view.timeoutVisible = true;
    PlayerUI_Update(view, start + 5000U);
    require(hasText("CANCELLED"), "External timeout did not replace confirmation");
}

void volumeValuesAndPartialChanges() {
    for (uint8_t volume : {uint8_t(0), uint8_t(50), uint8_t(100)}) {
        freshUi();
        auto view = playback(1, volume);
        PlayerUI_Update(view, 0);
        expectBar(167, volume, COLOR_CYAN);
        require(hasText("VOL " + std::to_string(volume) + " PCT"), "Normal volume number is incorrect");
        PlayerUI_VolumeChanged(100);
        calls.clear();
        PlayerUI_Update(view, 100);
        require(hasText("VOLUME") && textAt(72).text == std::to_string(volume),
                "Overlay does not show the supplied codec setting");
        expectBar(161, volume, COLOR_YELLOW);
        calls.clear();
        PlayerUI_Update(view, 1599);
        require(calls.empty() && hasText("VOLUME"), "Overlay ended before 1500 ms");
        PlayerUI_Update(view, 1600);
        require(hasText("NOW PLAYING"), "Overlay did not end at 1500 ms");
    }

    freshUi();
    auto view = playback(1, 50);
    PlayerUI_Update(view, 0);
    calls.clear();
    view.volume = 0;
    PlayerUI_Update(view, 10);
    expectPartial("Normal volume change cleared the screen");
    expectBar(167, 0, COLOR_CYAN);
    require(hasText("VOL 0 PCT") && !hasText("VOL 50 PCT"), "Volume decrease left old digits");
    PlayerUI_VolumeChanged(100);
    PlayerUI_Update(view, 100);
    calls.clear();
    view.volume = 100;
    PlayerUI_VolumeChanged(1300);
    PlayerUI_Update(view, 1300);
    expectPartial("Repeated overlay volume change cleared the screen");
    expectBar(161, 100, COLOR_YELLOW);
    require(textAt(72).text == "100", "Overlay did not update large digits");
    calls.clear();
    PlayerUI_Update(view, 2799);
    require(calls.empty() && hasText("VOLUME"), "Repeated tap did not restart overlay timer");
    PlayerUI_Update(view, 2800);
    require(hasText("NOW PLAYING") && hasText("VOL 100 PCT"), "Repeated overlay restored stale setting");
}

void overlayWrapAndInterruption() {
    freshUi();
    auto view = playback();
    const uint32_t start = UINT32_MAX - 400U;
    PlayerUI_Update(view, start - 1);
    PlayerUI_VolumeChanged(start);
    PlayerUI_Update(view, start);
    calls.clear();
    PlayerUI_Update(view, start + 1499U);
    require(calls.empty() && hasText("VOLUME"), "Overlay timing broke across tick wrap");
    PlayerUI_Update(view, start + 1500U);
    require(hasText("NOW PLAYING"), "Wrapped overlay did not expire");

    PlayerUI_VolumeChanged(100);
    PlayerUI_Update(view, 100);
    view.playback = UiPlaybackState::Paused;
    calls.clear();
    PlayerUI_Update(view, 200);
    expectPartial("Overlay state-label change cleared the screen");
    require(hasText("PAUSED - SONG 2"), "Overlay retained PLAY after a paused snapshot");
    PlayerUI_Update(view, 1600);
    require(hasText("PAUSED") && hasText("HOLD UP RESUME"), "Overlay expiry restored playing instead of paused");

    PlayerUI_VolumeChanged(2000);
    PlayerUI_Update(view, 2000);
    PlayerUI_CancelTransient();
    PlayerUI_Update(view, 2001);
    require(!hasText("VOLUME") && hasText("PAUSED"), "Explicit pause/resume cancellation did not dismiss overlay");

    PlayerUI_VolumeChanged(3000);
    PlayerUI_Update(view, 3000);
    view.playback = UiPlaybackState::Idle;
    view.currentSong = nullptr;
    PlayerUI_Update(view, 3001);
    require(hasText("SELECT SONG") && !hasText("VOLUME"), "EOF did not dismiss volume overlay");

    view = playback();
    PlayerUI_VolumeChanged(4000);
    PlayerUI_Update(view, 4000);
    view.selection = UiSelectionState::Release;
    view.candidateIndex = 3;
    view.candidateSong = songs[3];
    PlayerUI_Update(view, 4001);
    require(hasText("RELEASE ALL") && !hasText("VOLUME"), "Selection did not override volume overlay");
    view.selection = UiSelectionState::None;
    PlayerUI_Update(view, 4002);
    require(hasText("NOW PLAYING") && !hasText("VOLUME"), "Selection cancellation revived a stale overlay");
}

} // namespace

// These fixtures model LCD API boundaries, not the STM32 bus or font bitmaps.
void LCD_Fill(uint16_t color) {
    calls.push_back({DrawKind::FullFill, 0, 0, 240, 240, color, 0, ""});
    visibleText.clear();
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint16_t color) {
    require(width != 0 && height != 0, "LCD received a zero-area rectangle");
    require(x < 240 && y < 240 && uint32_t(x) + width <= 240 &&
            uint32_t(y) + height <= 240, "LCD rectangle escaped the 240x240 screen");
    const Draw draw = {DrawKind::Rectangle, x, y, width, height, color, 0, ""};
    calls.push_back(draw);
    eraseText(draw);
}

void LCD_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color,
                  uint8_t scale) {
    require(text != nullptr && scale != 0, "LCD received invalid text arguments");
    for (const char *c = text; *c; ++c) {
        require((*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                *c == '-' || *c == ' ', "LCD received a glyph absent from the real font");
    }
    const uint32_t width = std::strlen(text) * 6U * scale;
    require(x < 240 && y < 240 && x + width <= 240 && y + 7U * scale <= 240,
            "LCD text escaped the 240x240 screen");
    const Draw draw = {DrawKind::Text, x, y, static_cast<uint16_t>(width),
                       static_cast<uint16_t>(7 * scale), color, scale, text};
    calls.push_back(draw);
    if (*text) visibleText.push_back(draw);
}

int main() {
    struct Case { const char *name; void (*run)(); };
    const Case cases[] = {
        {"startup status timing", startupStatus},
        {"all eight actual metadata entries", realSongMetadata},
        {"future metadata bounds and glyph fallback", boundedFutureMetadata},
        {"unchanged snapshots and partial regions", unchangedAndPartialRegions},
        {"captured selection and real controller deadline", capturedSelectionAndDeadline},
        {"countdown wrap and controller ownership", countdownWrapAndControllerOwnership},
        {"volume values and partial changes", volumeValuesAndPartialChanges},
        {"overlay wrap and interruption", overlayWrapAndInterruption},
    };
    unsigned failures = 0;
    for (const auto &test : cases) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception &error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    if (failures) {
        std::cerr << failures << " UI case(s) failed\n";
        return 1;
    }
    std::cout << "All player UI host checks passed\n";
    return 0;
}
