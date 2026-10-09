#include "player_ui.h"
#include "lcd_display.h"

#include <cstdio>
#include <cstring>

namespace
{
constexpr uint16_t SCREEN_SIZE = 240;
constexpr uint16_t MARGIN = 12;
constexpr uint16_t CONTENT_WIDTH = SCREEN_SIZE - 2 * MARGIN;

enum class Screen { Unknown, Startup, Ready, Failure, Idle, Release, Confirm,
                    Playing, Paused, Timeout, Volume };
Screen renderedScreen = Screen::Unknown;
const Song *renderedSong = nullptr;
uint8_t renderedIndex = 0;
uint8_t renderedVolume = 0;
uint8_t renderedCountdown = 0;
UiPlaybackState renderedPlayback = UiPlaybackState::Idle;
uint32_t confirmationStart = 0;
uint32_t startupStatusStart = 0;
Screen startupStatus = Screen::Unknown;
bool volumeOverlay = false;
uint32_t volumeOverlayStart = 0;

// Safety belongs to the UI wrapper; the verified LCD primitives are unchanged.
void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
              uint16_t color)
{
    if (x >= SCREEN_SIZE || y >= SCREEN_SIZE || width == 0 || height == 0) return;
    if (width > SCREEN_SIZE - x) width = SCREEN_SIZE - x;
    if (height > SCREEN_SIZE - y) height = SCREEN_SIZE - y;
    LCD_FillRect(x, y, width, height, color);
}

void normalize(const char *source, char *destination, size_t capacity)
{
    if (capacity == 0) return;
    size_t length = 0;
    bool space = false;
    while (*source && length + 1 < capacity)
    {
        char c = *source++;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        const bool supported = (c >= 'A' && c <= 'Z') ||
                               (c >= '0' && c <= '9') || c == '-';
        if (!supported)
        {
            space = length != 0;
            continue;
        }
        if (space)
        {
            if (length + 2 >= capacity) break;
            destination[length++] = ' ';
        }
        destination[length++] = c;
        space = false;
    }
    destination[length] = '\0';
}

void centered(uint16_t y, const char *text, uint8_t scale, uint16_t color)
{
    if (scale == 0 || y + 7 * scale > SCREEN_SIZE) return;
    char bounded[37];
    normalize(text, bounded, sizeof(bounded));
    size_t length = std::strlen(bounded);
    while (scale > 1 && length * 6 * scale > CONTENT_WIDTH) --scale;
    const size_t capacity = CONTENT_WIDTH / (6 * scale);
    if (length > capacity)
    {
        length = capacity;
        bounded[length] = '\0';
    }
    const uint16_t width = length * 6 * scale;
    LCD_DrawText((SCREEN_SIZE - width) / 2, y, bounded, color, scale);
}

struct SongText
{
    char title[80];
    char composer[40];
    char lines[2][37];
    uint8_t scale;
};

bool wrapTitle(const char *title, size_t columns, char (&lines)[2][37])
{
    lines[0][0] = lines[1][0] = '\0';
    const size_t length = std::strlen(title);
    if (length <= columns)
    {
        std::strcpy(lines[0], title);
        return true;
    }
    // Choose the most balanced word boundary that fits both title lines.
    size_t split = 0;
    size_t bestDifference = length;
    for (size_t i = 1; i < length; ++i)
    {
        if (title[i] != ' ' || i > columns || length - i - 1 > columns) continue;
        const size_t right = length - i - 1;
        const size_t difference = i > right ? i - right : right - i;
        if (difference < bestDifference)
        {
            split = i;
            bestDifference = difference;
        }
    }
    if (split == 0) return false;
    std::memcpy(lines[0], title, split);
    lines[0][split] = '\0';
    std::strcpy(lines[1], title + split + 1);
    return true;
}

SongText formatSong(const Song &song)
{
    SongText text = {};
    char joined[160];
    std::snprintf(joined, sizeof(joined), "%s %s",
                  song.name1.c_str(), song.name2.c_str());
    char *separator = std::strchr(joined, '-');
    if (separator)
    {
        *separator = '\0';
        normalize(separator + 1, text.composer, sizeof(text.composer));
    }
    normalize(joined, text.title, sizeof(text.title));
    for (text.scale = 3; text.scale > 0; --text.scale)
    {
        if (wrapTitle(text.title, CONTENT_WIDTH / (6 * text.scale), text.lines))
            return text;
    }
    // Last resort for future metadata: bounded text using supported glyphs.
    text.scale = 1;
    std::snprintf(text.lines[0], sizeof(text.lines[0]), "%.36s", text.title);
    std::snprintf(text.lines[1], sizeof(text.lines[1]), "%.36s", text.title + 36);
    return text;
}

void songText(const Song &song, uint16_t firstY, uint16_t secondY,
              uint16_t composerY)
{
    const SongText text = formatSong(song);
    centered(firstY, text.lines[0], text.scale, COLOR_WHITE);
    centered(secondY, text.lines[1], text.scale, COLOR_WHITE);
    centered(composerY, text.composer, 2, COLOR_CYAN);
}

void heading(const char *text, uint16_t color)
{
    centered(12, text, 2, color);
    fillRect(MARGIN, 33, CONTENT_WIDTH, 1, COLOR_CYAN);
}

void playbackLabels(bool paused)
{
    centered(12, paused ? "PAUSED" : "NOW PLAYING", 2,
             paused ? COLOR_RED : COLOR_CYAN);
    centered(140, paused ? "PAUSED" : "PLAY", 2,
             paused ? COLOR_RED : COLOR_GREEN);
    centered(231, paused ? "HOLD UP RESUME" : "HOLD UP PAUSE", 1, COLOR_WHITE);
}

void volumeNumber(uint8_t volume)
{
    char text[20];
    std::snprintf(text, sizeof(text), "VOL %u PCT", volume);
    centered(190, text, 2, COLOR_YELLOW);
}

void volumeBar(uint16_t y, uint8_t volume, uint16_t color, bool outline = true)
{
    if (outline) fillRect(20, y, 200, 16, COLOR_WHITE);
    fillRect(22, y + 2, 196, 12, COLOR_BLACK);
    const uint16_t width = 196UL * volume / 100;
    fillRect(22, y + 2, width, 12, color); // Wrapper skips volume-zero fill.
}

void branding(const char *status, uint16_t color)
{
    LCD_Fill(COLOR_BLACK);
    centered(64, "RT-SPARK", 3, COLOR_CYAN);
    centered(104, "MP3 PLAYER", 3, COLOR_WHITE);
    centered(157, status, 2, color);
}

void displayPlayback(bool paused, const Song &song, uint8_t songIndex,
                     uint8_t volume)
{
    LCD_Fill(COLOR_BLACK);
    fillRect(MARGIN, 33, CONTENT_WIDTH, 1, COLOR_CYAN);
    songText(song, 53, 81, 113);
    playbackLabels(paused);
    volumeBar(165, volume, COLOR_CYAN);
    volumeNumber(volume);
    char text[24];
    std::snprintf(text, sizeof(text), "SONG %u - %u%u%u", songIndex + 1,
                  (songIndex >> 2) & 1, (songIndex >> 1) & 1, songIndex & 1);
    // Footer shares one row with compact volume hints.
    LCD_DrawText(12, 212, text, COLOR_CYAN, 1);
    LCD_DrawText(162, 212, "POT VOLUME", COLOR_WHITE, 1);
}

void largeVolumeNumber(uint8_t volume)
{
    fillRect(MARGIN, 70, CONTENT_WIDTH, 46, COLOR_BLACK);
    char text[8];
    std::snprintf(text, sizeof(text), "%u", volume);
    centered(72, text, 6, COLOR_YELLOW);
}

void volumeFooter(const PlayerUiView &view)
{
    fillRect(MARGIN, 224, CONTENT_WIDTH, 9, COLOR_BLACK);
    char text[24];
    std::snprintf(text, sizeof(text), "%s - SONG %u",
                  view.playback == UiPlaybackState::Paused ? "PAUSED" : "PLAY",
                  view.currentIndex + 1);
    centered(225, text, 1, COLOR_CYAN);
}

void displayVolume(const PlayerUiView &view)
{
    LCD_Fill(COLOR_BLACK);
    centered(24, "VOLUME", 2, COLOR_CYAN);
    largeVolumeNumber(view.volume);
    centered(123, "PCT", 2, COLOR_WHITE);
    volumeBar(159, view.volume, COLOR_YELLOW);
    centered(208, "TURN POT FOR VOLUME", 1, COLOR_WHITE);
    volumeFooter(view);
}

void bitBoxes(uint8_t bits, uint8_t previousBits = 0, bool partial = false)
{
    const char *labels[] = {"DOWN", "LEFT", "RIGHT"};
    for (uint8_t i = 0; i < 3; ++i)
    {
        if (partial && ((bits ^ previousBits) & (4 >> i)) == 0) continue;
        const uint16_t x = 52 + i * 50;
        const bool pressed = (bits & (4 >> i)) != 0;
        fillRect(x, 42, 36, 30, pressed ? COLOR_CYAN : COLOR_WHITE);
        if (!pressed) fillRect(x + 1, 43, 34, 28, COLOR_BLACK);
        const char digit[] = {pressed ? '1' : '0', '\0'};
        LCD_DrawText(x + 9, 46, digit, pressed ? COLOR_BLACK : COLOR_WHITE, 3);
        LCD_DrawText(x + (36 - std::strlen(labels[i]) * 6) / 2, 78,
                     labels[i], COLOR_WHITE, 1);
    }
}

void countdown(uint8_t seconds)
{
    fillRect(MARGIN, 228, CONTENT_WIDTH, 9, COLOR_BLACK);
    char text[12];
    std::snprintf(text, sizeof(text), "%u SEC", seconds);
    centered(229, text, 1, COLOR_YELLOW);
}

void selectionMetadata(const PlayerUiView &view)
{
    char text[16];
    std::snprintf(text, sizeof(text), "SONG %u", view.candidateIndex + 1);
    centered(94, text, 2, COLOR_CYAN);
    if (view.candidateSong) songText(*view.candidateSong, 115, 143, 177);
}

void selectionAction(Screen screen, uint8_t seconds)
{
    fillRect(MARGIN, 202, CONTENT_WIDTH, 18, COLOR_BLACK);
    fillRect(MARGIN, 228, CONTENT_WIDTH, 9, COLOR_BLACK);
    if (screen == Screen::Release)
    {
        centered(204, "RELEASE ALL", 2, COLOR_YELLOW);
        centered(229, "THEN UP CONFIRM", 1, COLOR_WHITE);
    }
    else if (screen == Screen::Confirm)
    {
        centered(204, "UP CONFIRM", 2, COLOR_GREEN);
        countdown(seconds);
    }
    else
    {
        centered(204, "UP SELECT", 2, COLOR_GREEN);
        centered(229, "HOLD BITS AND PRESS UP", 1, COLOR_WHITE);
    }
}

void displaySelection(const PlayerUiView &view, Screen screen, uint8_t seconds)
{
    LCD_Fill(COLOR_BLACK);
    heading(screen == Screen::Idle ? "SELECT SONG" : "CONFIRM SONG", COLOR_CYAN);
    bitBoxes(view.candidateIndex);
    selectionMetadata(view);
    selectionAction(screen, seconds);
}

void displayTimeout()
{
    LCD_Fill(COLOR_BLACK);
    centered(64, "CANCELLED", 3, COLOR_RED);
    centered(114, "SELECTION TIMEOUT", 2, COLOR_WHITE);
    centered(153, "NO SONG CHANGED", 2, COLOR_WHITE);
    centered(194, "RETURNING TO PLAYER", 1, COLOR_CYAN);
}

bool isPlayback(Screen screen)
{
    return screen == Screen::Playing || screen == Screen::Paused;
}

bool isSelection(Screen screen)
{
    return screen == Screen::Idle || screen == Screen::Release || screen == Screen::Confirm;
}

void updatePlaybackVolume(uint8_t volume)
{
    volumeBar(165, volume, COLOR_CYAN, false);
    fillRect(MARGIN, 188, CONTENT_WIDTH, 17, COLOR_BLACK);
    volumeNumber(volume);
}

void updatePlaybackLabels(bool paused)
{
    fillRect(MARGIN, 12, CONTENT_WIDTH, 14, COLOR_BLACK);
    fillRect(MARGIN, 140, CONTENT_WIDTH, 14, COLOR_BLACK);
    fillRect(MARGIN, 231, CONTENT_WIDTH, 7, COLOR_BLACK);
    playbackLabels(paused);
}
} // namespace

void PlayerUI_ShowStartup()
{
    PlayerUI_CancelTransient();
    renderedScreen = Screen::Startup;
    renderedSong = nullptr;
    renderedPlayback = UiPlaybackState::Idle;
    branding("INITIALIZING", COLOR_YELLOW);
}

void PlayerUI_ShowAudioReady(uint32_t now)
{
    startupStatusStart = now;
    startupStatus = renderedScreen = Screen::Ready;
    branding("AUDIO READY", COLOR_GREEN);
}

void PlayerUI_ShowAudioFailure(uint32_t now)
{
    startupStatusStart = now;
    startupStatus = renderedScreen = Screen::Failure;
    branding("INIT FAILED", COLOR_RED);
}

void PlayerUI_ConfirmationReady(uint32_t inputNow)
{
    confirmationStart = inputNow;
}

void PlayerUI_VolumeChanged(uint32_t now)
{
    volumeOverlay = true;
    volumeOverlayStart = now;
}

void PlayerUI_CancelTransient()
{
    startupStatus = Screen::Unknown;
    volumeOverlay = false;
}

void PlayerUI_Update(const PlayerUiView &view, uint32_t now)
{
    if (volumeOverlay && (now - volumeOverlayStart >= 1500 ||
                          view.playback == UiPlaybackState::Idle))
        volumeOverlay = false;
    if (startupStatus != Screen::Unknown && now - startupStatusStart >= 600)
        startupStatus = Screen::Unknown;
    Screen screen;
    if (view.selection != UiSelectionState::None)
    {
        PlayerUI_CancelTransient();
        screen = view.selection == UiSelectionState::Release
                 ? Screen::Release : Screen::Confirm;
    }
    else if (view.timeoutVisible) screen = Screen::Timeout;
    else if (volumeOverlay) screen = Screen::Volume;
    else if (startupStatus != Screen::Unknown)
        screen = startupStatus;
    else if (view.playback == UiPlaybackState::Playing) screen = Screen::Playing;
    else if (view.playback == UiPlaybackState::Paused) screen = Screen::Paused;
    else screen = Screen::Idle;

    const bool selectionScreen = isSelection(screen);
    const Song *song = selectionScreen ? view.candidateSong : view.currentSong;
    const uint8_t index = selectionScreen ? view.candidateIndex : view.currentIndex;
    const uint32_t elapsed = now - confirmationStart;
    // Controller events own expiry. Never invent a zero/extend its deadline.
    const uint8_t seconds = elapsed >= 5000 ? 1 : (5000 - elapsed + 999) / 1000;
    const bool metadataChanged = song != renderedSong || index != renderedIndex;
    const bool volumeChanged = view.volume != renderedVolume;
    const bool sameSelectionLayout = selectionScreen && isSelection(renderedScreen);
    const bool samePlaybackLayout = isPlayback(screen) && isPlayback(renderedScreen);
    const bool fullRedraw =
        (screen != renderedScreen && !sameSelectionLayout && !samePlaybackLayout) ||
        (isPlayback(screen) && metadataChanged) ||
        (screen == Screen::Volume && metadataChanged);
    if (fullRedraw)
    {
        if (selectionScreen) displaySelection(view, screen, seconds);
        else if ((screen == Screen::Playing || screen == Screen::Paused) && song)
            displayPlayback(screen == Screen::Paused, *song, index, view.volume);
        else if (screen == Screen::Timeout) displayTimeout();
        else if (screen == Screen::Volume) displayVolume(view);
    }
    else if (selectionScreen)
    {
        if (metadataChanged)
        {
            bitBoxes(index, renderedIndex, true);
            fillRect(MARGIN, 94, CONTENT_WIDTH, 14, COLOR_BLACK);
            fillRect(MARGIN, 115, CONTENT_WIDTH, 76, COLOR_BLACK);
            selectionMetadata(view);
        }
        if (screen != renderedScreen)
        {
            fillRect(MARGIN, 12, CONTENT_WIDTH, 14, COLOR_BLACK);
            centered(12, screen == Screen::Idle ? "SELECT SONG" : "CONFIRM SONG",
                     2, COLOR_CYAN);
            selectionAction(screen, seconds);
        }
        else if (screen == Screen::Confirm && seconds != renderedCountdown)
            countdown(seconds);
    }
    else if (isPlayback(screen))
    {
        if (screen != renderedScreen) updatePlaybackLabels(screen == Screen::Paused);
        if (volumeChanged) updatePlaybackVolume(view.volume);
    }
    else if (screen == Screen::Volume)
    {
        if (volumeChanged)
        {
            largeVolumeNumber(view.volume);
            volumeBar(159, view.volume, COLOR_YELLOW, false);
        }
        if (view.playback != renderedPlayback) volumeFooter(view);
    }

    renderedScreen = screen;
    renderedSong = song;
    renderedIndex = index;
    renderedVolume = view.volume; // Last-drawn cache only; never controls codec.
    renderedCountdown = seconds;
    renderedPlayback = view.playback;
}
