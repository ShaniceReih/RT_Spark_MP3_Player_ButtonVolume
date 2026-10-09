#include "player_ui.h"
#include "lcd_display.h"
#include "song_selection.h"
#include "song_def.h"

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
unsigned checks = 0;

constexpr uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
    return static_cast<uint16_t>(((r & 248U) << 8) | ((g & 252U) << 3) | (b >> 3));
}
constexpr uint16_t themeBackground = rgb565(20, 12, 36);
constexpr uint16_t themePurple = rgb565(167, 120, 244);
constexpr uint16_t themeLavender = rgb565(216, 188, 255);

void require(bool condition, const char *message) {
    ++checks;
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

bool hasPart(const std::string &text) {
    return std::any_of(visibleText.begin(), visibleText.end(),
        [&](const Draw &draw) { return draw.text.find(text) != std::string::npos; });
}

bool overlayVisible() {
    return std::any_of(visibleText.begin(), visibleText.end(),
        [](const Draw &draw) { return draw.text == "VOLUME" && draw.y == 44; });
}

std::string titleText(unsigned firstY, unsigned lastY) {
    std::vector<Draw> rows;
    for (const Draw &draw : visibleText) {
        if (draw.y >= firstY && draw.y <= lastY && !draw.text.empty()) rows.push_back(draw);
    }
    std::sort(rows.begin(), rows.end(), [](const Draw &a, const Draw &b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    std::string result;
    for (const Draw &draw : rows) {
        if (!result.empty()) result += ' ';
        result += draw.text;
    }
    return result;
}

unsigned fullFills() {
    return static_cast<unsigned>(std::count_if(calls.begin(), calls.end(),
        [](const Draw &draw) { return draw.kind == DrawKind::FullFill; }));
}

void expectPartial(const char *message) {
    require(!calls.empty(), "Expected a visible update");
    require(fullFills() == 0, message);
}

void expectCountdownOnly() {
    expectPartial("Countdown cleared the entire confirmation screen");
    for (const Draw &draw : calls) {
        require(draw.y >= 217 && draw.y + draw.height <= 233,
                "Countdown redrew unrelated metadata or instructions");
    }
}

void freshUi() {
    PlayerUI_CancelTransient();
    PlayerUI_ShowStartup();
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

void expectBar(uint16_t x, uint16_t y, uint16_t totalWidth, uint16_t height,
               uint8_t volume) {
    const uint16_t expected = totalWidth * volume / 100U;
    bool cleared = false;
    unsigned fills = 0;
    for (const Draw &draw : calls) {
        if (draw.kind != DrawKind::Rectangle || draw.x != x || draw.y != y ||
            draw.height != height) continue;
        const bool accent = draw.color == themePurple || draw.color == themeLavender;
        if (!accent && draw.width == totalWidth) cleared = true;
        if (!accent) continue;
        require(cleared, "Volume fill did not first clear the previous bar interior");
        require(draw.width == expected, "Volume bar differs from the accepted volume");
        ++fills;
    }
    require(cleared, "Volume bar interior was not cleared");
    require(fills == (volume == 0 ? 0U : 1U), "Volume fill missing or duplicated");
}

void startupThemeAndStatus() {
    freshUi();
    PlayerUI_ShowStartup();
    require(fullFills() == 1, "Startup did not draw exactly one background");
    require(calls.front().color == themeBackground, "Startup is not using dark purple");
    require(hasText("RT-SPARK") && hasText("MP3 PLAYER") && hasText("INITIALIZING"),
            "Startup branding or initialization status missing");
    const auto view = candidate(0);
    PlayerUI_ShowAudioReady(100);
    require(hasText("AUDIO READY"), "Audio-ready status missing");
    calls.clear();
    PlayerUI_Update(view, 699);
    require(calls.empty() && hasText("AUDIO READY"), "Ready status ended before 600 ms");
    PlayerUI_Update(view, 700);
    require(hasPart("SELECT") || hasPart("CHOOSE"), "Ready status did not restore song selection");

    const uint32_t start = UINT32_MAX - 300U;
    PlayerUI_ShowAudioFailure(start);
    require(hasText("INIT FAILED"), "Audio failure status missing");
    calls.clear();
    PlayerUI_Update(view, start + 599U);
    require(calls.empty(), "Failure status timing broke across tick wrap");
    PlayerUI_Update(view, start + 600U);
    require(hasPart("SELECT") || hasPart("CHOOSE"), "Failure status did not restore selection");
}

void allActualMetadataAndBits() {
    const char *titles[] = {"FUR ELISE", "CANON IN D", "MINUET IN G MAJOR",
                           "TURKISH MARCH", "NOCTURNE IN E FLAT", "WALTZ NO 2",
                           "NOCTURNE IN C SHARP", "SYMPHONY NO 40"};
    const char *composers[] = {"BEETHOVEN", "PACHEBELBEL", "BACH", "MOZART", "CHOPIN",
                              "SHOSTAKOVICH", "CHOPIN", "MOZART"};
    for (uint8_t index = 0; index < 8; ++index) {
        freshUi();
        auto view = playback(index);
        PlayerUI_Update(view, 100);
        require(titleText(108, 134) == titles[index], "Playback title lost or altered metadata");
        require(hasText(composers[index]), "Playback composer spelling changed");
        require(hasText("NOW PLAYING"), "Playback status missing");
        calls.clear();
        PlayerUI_Update(view, 120);
        PlayerUI_Update(view, 1000);
        require(calls.empty(), "Unchanged playing snapshot redrew");

        freshUi();
        PlayerUI_Update(candidate(index), 100);
        require(titleText(119, 143) == titles[index], "Preview title differs from the actual song");
        require(hasText(composers[index]), "Preview composer changed");
        for (uint8_t bit = 0; bit < 3; ++bit) {
            const uint16_t left = 46 + 52 * bit;
            const auto found = std::find_if(visibleText.begin(), visibleText.end(),
                [&](const Draw &draw) {
                    return draw.x >= left && draw.x < left + 44 &&
                           draw.y >= 46 && draw.y < 80 && draw.text.size() == 1;
                });
            require(found != visibleText.end(), "Binary card has no visible digit");
            require(found->text == ((index & (4 >> bit)) ? "1" : "0"),
                    "Binary cards changed DOWN LEFT RIGHT significance");
        }
        require(hasText("DOWN") && hasText("LEFT") && hasText("RIGHT"),
                "Binary card control labels missing");
        calls.clear();
        PlayerUI_Update(candidate(index), 120);
        require(calls.empty(), "Unchanged preview snapshot redrew");
    }
}

void longAndUnsupportedMetadata() {
    Song future("Extraordinarilylongwordabcdefghijklmnopqrstuvwxyz0123456789",
                "evenmoreletters - Composer with unsupported %!? punctuation", nullptr,
                nullptr, 0.1f, 0);
    freshUi();
    auto view = playback();
    view.currentSong = &future;
    PlayerUI_Update(view, 100);
    const auto title = std::find_if(visibleText.begin(), visibleText.end(),
        [](const Draw &draw) { return draw.y == 108 && !draw.text.empty(); });
    require(title != visibleText.end(), "Long future title disappeared");
    require(title->scale < 3, "Long future title did not shrink to fit");
    require(!titleText(108, 134).empty(), "Long title rendering has no text");
    require(std::any_of(visibleText.begin(), visibleText.end(),
        [](const Draw &draw) { return draw.y == 166 && !draw.text.empty(); }),
        "Long future composer disappeared");
    // The fake LCD checks every call against bounds and the real 5x7 alphabet.
}

void partialMetadataAndPlaybackTransitions() {
    freshUi();
    auto idle = candidate(0);
    PlayerUI_Update(idle, 0);
    calls.clear();
    PlayerUI_Update(idle, 20);
    require(calls.empty(), "Unchanged idle preview redrew at the task cadence");
    idle = candidate(7);
    PlayerUI_Update(idle, 40);
    expectPartial("Preview change cleared the full display");
    require(titleText(119, 143) == "SYMPHONY NO 40" && hasText("MOZART"),
            "New preview metadata missing");
    require(!hasText("FUR ELISE") && !hasText("BEETHOVEN"), "Old preview metadata survived");

    freshUi();
    auto view = playback();
    PlayerUI_Update(view, 0);
    calls.clear();
    view.playback = UiPlaybackState::Paused;
    PlayerUI_Update(view, 100);
    expectPartial("Pause cleared the complete playback layout");
    require(hasText("PAUSED"), "Pause state or resume hint missing");
    require(!hasText("NOW PLAYING"), "Playing label survived pause");
    require(titleText(108, 134) == "CANON IN D", "Pause erased the song title");
    calls.clear();
    PlayerUI_Update(view, 120);
    require(calls.empty(), "Unchanged pause redrew");
    view.playback = UiPlaybackState::Playing;
    PlayerUI_Update(view, 200);
    expectPartial("Resume cleared the entire playback layout");
    require(hasText("NOW PLAYING"), "Resume state or pause hint missing");
    require(!hasText("PAUSED"), "Paused labels survived resume");
}

SelectionButtons chord(uint8_t bits, bool up) {
    SelectionButtons buttons;
    buttons.up = up;
    buttons.down = (bits & 4) != 0;
    buttons.left = (bits & 2) != 0;
    buttons.right = (bits & 1) != 0;
    return buttons;
}

void capturedSelectionAndRealDeadline() {
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
    require(sample(65, chord(3, true)).captured, "Real controller did not capture selection");
    require(hasPart("RELEASE"), "Captured screen lost the release-all requirement");
    require(view.candidateIndex == 3 && titleText(119, 143) == "TURKISH MARCH",
            "Captured candidate differs from controller");
    calls.clear();
    sample(6000, chord(3, true));
    require(calls.empty(), "Held capture began a visual timeout");
    sample(6010, chord(5, false));
    sample(6035, chord(5, false));
    require(controller.waitingForRelease(), "Release gate ignored held bit buttons");
    require(view.candidateIndex == 3 && titleText(119, 143) == "TURKISH MARCH",
            "Changed raw bits replaced the frozen candidate");
    sample(6100, {});
    require(sample(6125, {}).readyToConfirm, "Release-all did not begin confirmation");
    require(hasText("5 SEC") && hasPart("UP"), "Countdown or confirmation instruction missing");
    require(!hasPart("RELEASE"), "Release instruction survived confirmation");
    calls.clear();
    sample(7124, {});
    require(calls.empty(), "Countdown changed before the first full second");
    for (uint32_t second = 1; second <= 4; ++second) {
        calls.clear();
        sample(6125 + second * 1000U, {});
        expectCountdownOnly();
        require(hasText(std::to_string(5 - second) + " SEC"), "Countdown missed exact boundary");
    }
    calls.clear();
    sample(11124, {});
    require(calls.empty() && hasText("1 SEC"), "Confirmation ended before its deadline");
    require(sample(11125, {}).timedOut, "Controller no longer owns exact timeout");
    require(hasText("CANCELLED"), "Timeout screen missing");
    calls.clear();
    PlayerUI_Update(view, 999999);
    require(calls.empty() && hasText("CANCELLED"), "UI independently dismissed timeout notice");
    view.timeoutVisible = false;
    PlayerUI_Update(view, 1000000);
    require(hasText("NOW PLAYING"), "Timeout did not restore current playback state");
}

void countdownWrapAndAuthority() {
    freshUi();
    auto view = candidate(7, UiSelectionState::Confirm);
    const uint32_t start = UINT32_MAX - 1500U;
    PlayerUI_ConfirmationReady(start);
    PlayerUI_Update(view, start);
    require(hasText("5 SEC"), "Wrapped confirmation did not begin at five");
    for (uint32_t second = 1; second <= 4; ++second) {
        calls.clear();
        PlayerUI_Update(view, start + second * 1000U);
        expectCountdownOnly();
        require(hasText(std::to_string(5 - second) + " SEC"), "Countdown broke at tick wrap");
    }
    calls.clear();
    PlayerUI_Update(view, start + 5000U);
    require(calls.empty() && hasText("1 SEC"), "UI invented timeout independently");
    view.selection = UiSelectionState::None;
    view.timeoutVisible = true;
    PlayerUI_Update(view, start + 5000U);
    require(hasText("CANCELLED"), "Controller timeout did not replace confirmation");
}

void acceptedVolumeGeometryAndTiming() {
    for (uint8_t volume : {uint8_t(0), uint8_t(8), uint8_t(50), uint8_t(100)}) {
        freshUi();
        auto view = playback(1, volume);
        PlayerUI_Update(view, 0);
        expectBar(18, 228, 204, 6, volume);
        PlayerUI_VolumeChanged(100);
        calls.clear();
        PlayerUI_Update(view, 100);
        require(overlayVisible() && hasText(std::to_string(volume)),
                "Volume overlay differs from supplied accepted value");
        expectBar(32, 168, 176, 10, volume);
        calls.clear();
        PlayerUI_Update(view, 1599);
        require(calls.empty() && overlayVisible(), "Volume overlay ended early");
        PlayerUI_Update(view, 1600);
        require(hasText("NOW PLAYING"), "Overlay did not end after 1500 ms");
    }

    freshUi();
    auto view = playback(1, 50);
    PlayerUI_Update(view, 0);
    calls.clear();
    view.volume = 0;
    PlayerUI_Update(view, 10);
    expectPartial("Accepted volume update cleared the entire playback screen");
    expectBar(18, 228, 204, 6, 0);
    PlayerUI_VolumeChanged(100);
    PlayerUI_Update(view, 100);
    calls.clear();
    view.volume = 100;
    PlayerUI_VolumeChanged(1300);
    PlayerUI_Update(view, 1300);
    expectPartial("Repeated overlay volume update cleared the full display");
    expectBar(32, 168, 176, 10, 100);
    require(hasText("100") && !hasText("0"), "Overlay left stale large digits");
    calls.clear();
    PlayerUI_Update(view, 2799);
    require(calls.empty() && overlayVisible(), "Latest accepted change did not extend overlay");
    PlayerUI_Update(view, 2800);
    require(hasText("NOW PLAYING"), "Repeated overlay did not restore playback");
}

void overlayWrapRestorationAndPriority() {
    freshUi();
    auto view = playback();
    const uint32_t start = UINT32_MAX - 400U;
    PlayerUI_Update(view, start - 1U);
    PlayerUI_VolumeChanged(start);
    PlayerUI_Update(view, start);
    calls.clear();
    PlayerUI_Update(view, start + 1499U);
    require(calls.empty() && overlayVisible(), "Overlay timing broke across tick wrap");
    PlayerUI_Update(view, start + 1500U);
    require(hasText("NOW PLAYING"), "Wrapped overlay did not expire");

    PlayerUI_VolumeChanged(100);
    PlayerUI_Update(view, 100);
    view.playback = UiPlaybackState::Paused;
    calls.clear();
    PlayerUI_Update(view, 200);
    expectPartial("Overlay paused-state change cleared the entire screen");
    require(hasPart("PAUSED"), "Overlay retained playing status after pause");
    PlayerUI_Update(view, 1600);
    require(hasText("PAUSED"), "Overlay restored playing instead of paused");

    PlayerUI_VolumeChanged(2000);
    PlayerUI_Update(view, 2000);
    PlayerUI_CancelTransient();
    PlayerUI_Update(view, 2001);
    require(!overlayVisible() && hasText("PAUSED"), "Explicit cancellation left volume overlay");

    PlayerUI_VolumeChanged(3000);
    PlayerUI_Update(view, 3000);
    view.playback = UiPlaybackState::Idle;
    view.currentSong = nullptr;
    PlayerUI_Update(view, 3001);
    require((hasPart("SELECT") || hasPart("CHOOSE")) && !overlayVisible(),
            "Song completion did not dismiss overlay");

    view = playback();
    PlayerUI_VolumeChanged(4000);
    PlayerUI_Update(view, 4000);
    view.selection = UiSelectionState::Release;
    view.candidateIndex = 3;
    view.candidateSong = songs[3];
    PlayerUI_Update(view, 4001);
    require(hasPart("RELEASE") && !overlayVisible(), "Selection did not override overlay");
    view.selection = UiSelectionState::None;
    PlayerUI_Update(view, 4002);
    require(hasText("NOW PLAYING") && !overlayVisible(), "Selection cancellation revived overlay");
}

} // namespace

// Validate the public LCD API. This does not simulate the LCD bus or glyph pixels.
void LCD_Fill(uint16_t color) {
    calls.push_back({DrawKind::FullFill, 0, 0, 240, 240, color, 0, ""});
    visibleText.clear();
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint16_t color) {
    require(width != 0 && height != 0, "LCD received a zero-area rectangle");
    require(x < 240 && y < 240 && uint32_t(x) + width <= 240 &&
            uint32_t(y) + height <= 240, "Rectangle escaped the 240x240 display");
    const Draw draw = {DrawKind::Rectangle, x, y, width, height, color, 0, ""};
    calls.push_back(draw);
    eraseText(draw);
}

void LCD_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color,
                  uint8_t scale) {
    require(text != nullptr && scale != 0, "LCD received invalid text arguments");
    for (const char *c = text; *c; ++c) {
        require((*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                *c == '-' || *c == ' ', "Text glyph absent from actual 5x7 font");
    }
    const uint32_t width = std::strlen(text) * 6U * scale;
    require(x < 240 && y < 240 && x + width <= 240 && y + 7U * scale <= 240,
            "Text escaped the 240x240 display");
    const Draw draw = {DrawKind::Text, x, y, static_cast<uint16_t>(width),
                       static_cast<uint16_t>(7 * scale), color, scale, text};
    calls.push_back(draw);
    if (*text) visibleText.push_back(draw);
}

int main() {
    struct Case { const char *name; void (*run)(); };
    const Case cases[] = {
        {"purple startup and existing status timing", startupThemeAndStatus},
        {"all eight real titles composers and bitcards", allActualMetadataAndBits},
        {"future long metadata and font bounds", longAndUnsupportedMetadata},
        {"partial preview pause and resume", partialMetadataAndPlaybackTransitions},
        {"captured selection and real controller deadline", capturedSelectionAndRealDeadline},
        {"countdown wrap and controller authority", countdownWrapAndAuthority},
        {"accepted volume geometry and overlay timing", acceptedVolumeGeometryAndTiming},
        {"overlay wrap restoration and selection priority", overlayWrapRestorationAndPriority},
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
    std::cout << sizeof(cases) / sizeof(cases[0]) << " groups, " << checks << " checks\n";
    if (failures) return 1;
    std::cout << "All purple PlayerUI host checks passed\n";
    return 0;
}
