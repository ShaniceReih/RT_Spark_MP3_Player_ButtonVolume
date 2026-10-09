#if defined(PLAYER_UI_PURPLE_THEME) && PLAYER_UI_PURPLE_THEME

#include "player_ui.h"
#include "lcd_display.h"

#include <cstdio>
#include <cstring>

namespace
{
constexpr uint16_t SCREEN_SIZE = 240;
constexpr uint16_t MARGIN = 12;
constexpr uint16_t CONTENT_WIDTH = SCREEN_SIZE - 2 * MARGIN;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return static_cast<uint16_t>(((red & 0xF8U) << 8) |
                                 ((green & 0xFCU) << 3) | (blue >> 3));
}

constexpr uint16_t UI_BG = rgb565(20, 12, 36);
constexpr uint16_t UI_PANEL = rgb565(40, 23, 66);
constexpr uint16_t UI_PURPLE = rgb565(167, 120, 244);
constexpr uint16_t UI_LAVENDER = rgb565(216, 188, 255);
constexpr uint16_t UI_PINK = rgb565(255, 167, 216);
constexpr uint16_t UI_TEXT = COLOR_WHITE;
constexpr uint16_t UI_TEXT_DIM = rgb565(195, 175, 225);
constexpr uint16_t UI_MUTED = rgb565(105, 84, 133);

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

// Keep clipping in the presentation wrapper, never in the verified driver.
void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
              uint16_t color)
{
    if (x >= SCREEN_SIZE || y >= SCREEN_SIZE || width == 0 || height == 0) return;
    if (width > SCREEN_SIZE - x) width = SCREEN_SIZE - x;
    if (height > SCREEN_SIZE - y) height = SCREEN_SIZE - y;
    LCD_FillRect(x, y, width, height, color);
}

// Four stepped corner bands give cards a soft outline without a new library.
void roundedPanel(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint16_t color)
{
    if (width < 10 || height < 8)
    {
        fillRect(x, y, width, height, color);
        return;
    }
    // Nonoverlapping bands draw each pixel once, even on the large overlay.
    fillRect(x + 4, y, width - 8, 1, color);
    fillRect(x + 2, y + 1, width - 4, 1, color);
    fillRect(x + 1, y + 2, width - 2, 2, color);
    fillRect(x, y + 4, width, height - 8, color);
    fillRect(x + 1, y + height - 4, width - 2, 2, color);
    fillRect(x + 2, y + height - 2, width - 4, 1, color);
    fillRect(x + 4, y + height - 1, width - 8, 1, color);
}

void DrawMusicNote(uint16_t x, uint16_t y, uint8_t scale, uint16_t color)
{
    fillRect(x + 6 * scale, y, 2 * scale, 12 * scale, color);
    fillRect(x + scale, y + 10 * scale, 7 * scale, scale, color);
    fillRect(x, y + 11 * scale, 9 * scale, 3 * scale, color);
    fillRect(x + scale, y + 14 * scale, 7 * scale, scale, color);
    fillRect(x + 8 * scale, y + scale, 4 * scale, 2 * scale, color);
    fillRect(x + 10 * scale, y + 3 * scale, 3 * scale, 2 * scale, color);
}

void DrawPlayIcon(uint16_t x, uint16_t y, uint8_t height, uint16_t color)
{
    for (uint8_t row = 0; row < height; ++row)
    {
        const uint8_t edge = row < height / 2 ? row : height - row - 1;
        fillRect(x, y + row, edge + 1, 1, color);
    }
}

void DrawPauseIcon(uint16_t x, uint16_t y, uint8_t height, uint16_t color)
{
    fillRect(x, y, 3, height, color);
    fillRect(x + 6, y, 3, height, color);
}

void DrawSpeakerIcon(uint16_t x, uint16_t y, uint8_t scale, uint16_t color)
{
    fillRect(x, y + 5 * scale, 3 * scale, 6 * scale, color);
    fillRect(x + 3 * scale, y + 3 * scale, 2 * scale, 10 * scale, color);
    fillRect(x + 5 * scale, y + scale, 2 * scale, 14 * scale, color);
    fillRect(x + 9 * scale, y + 4 * scale, scale, 8 * scale, color);
    fillRect(x + 8 * scale, y + 3 * scale, scale, 2 * scale, color);
    fillRect(x + 8 * scale, y + 11 * scale, scale, 2 * scale, color);
    fillRect(x + 12 * scale, y + 2 * scale, scale, 12 * scale, color);
    fillRect(x + 11 * scale, y + scale, scale, 2 * scale, color);
    fillRect(x + 11 * scale, y + 13 * scale, scale, 2 * scale, color);
}

void DrawSparkle(uint16_t x, uint16_t y, uint8_t size, uint16_t color)
{
    fillRect(x, y + size, 2 * size + 1, 1, color);
    fillRect(x + size, y, 1, 2 * size + 1, color);
    if (size > 2) fillRect(x + size - 1, y + size - 1, 3, 3, color);
}

// The existing 5x7 font has no percent glyph; draw one as a tiny icon.
void DrawPercent(uint16_t x, uint16_t y, uint8_t scale, uint16_t color)
{
    fillRect(x, y, 3 * scale, 3 * scale, color);
    fillRect(x + 11 * scale, y + 11 * scale, 3 * scale, 3 * scale, color);
    for (uint8_t i = 0; i < 12; ++i)
        fillRect(x + (11 - i) * scale, y + i * scale,
                 2 * scale, 2 * scale, color);
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
    text.scale = 1;
    std::snprintf(text.lines[0], sizeof(text.lines[0]), "%.36s", text.title);
    std::snprintf(text.lines[1], sizeof(text.lines[1]), "%.36s", text.title + 36);
    return text;
}

void songText(const Song &song, uint16_t firstY, uint16_t secondY,
              uint16_t composerY)
{
    const SongText text = formatSong(song);
    centered(firstY + (text.lines[1][0] == '\0' ? 8 : 0),
             text.lines[0], text.scale, UI_TEXT);
    centered(secondY, text.lines[1], text.scale, UI_TEXT);
    centered(composerY, text.composer, 1, UI_TEXT_DIM);
}

void heading(const char *text)
{
    centered(12, text, 2, UI_LAVENDER);
    DrawMusicNote(16, 8, 1, UI_PINK);
    DrawMusicNote(211, 8, 1, UI_PURPLE);
}

void selectionHeading(Screen screen)
{
    fillRect(MARGIN, 8, CONTENT_WIDTH, 31, UI_BG);
    heading(screen == Screen::Idle ? "MP3 PLAYER" :
            screen == Screen::Release ? "SONG CAPTURED" : "CONFIRM SONG");
    centered(32, screen == Screen::Idle ? "READY TO PLAY" :
             screen == Screen::Release ? "WAITING FOR RELEASE" :
             "PRESS UP TO PLAY", 1, UI_TEXT_DIM);
}

void playbackLabels(bool paused)
{
    roundedPanel(34, 180, 172, 24, UI_PANEL);
    const char *text = paused ? "PAUSED" : "NOW PLAYING";
    const uint16_t x = (SCREEN_SIZE - (std::strlen(text) * 12 + 18)) / 2;
    if (paused) DrawPauseIcon(x, 185, 13, UI_PINK);
    else DrawPlayIcon(x, 185, 13, UI_PURPLE);
    LCD_DrawText(x + 18, 185, text, paused ? UI_TEXT_DIM : UI_LAVENDER, 2);
}

void volumeNumber(uint8_t volume)
{
    char text[8];
    std::snprintf(text, sizeof(text), "%u", volume);
    LCD_DrawText(18, 207, "VOLUME", UI_TEXT_DIM, 1);
    LCD_DrawText(202 - std::strlen(text) * 12, 204, text, UI_LAVENDER, 2);
    DrawPercent(208, 204, 1, UI_LAVENDER);
}

void volumeBar(uint16_t y, uint8_t volume, bool outline = true)
{
    const bool overlay = y == 166;
    const uint16_t x = overlay ? 30 : 16;
    const uint16_t outerWidth = overlay ? 180 : 208;
    const uint16_t outerHeight = overlay ? 14 : 10;
    const uint16_t width = outerWidth - 4;
    const uint16_t height = outerHeight - 4;
    if (outline) roundedPanel(x, y, outerWidth, outerHeight, UI_MUTED);
    fillRect(x + 2, y + 2, width, height, UI_PANEL);
    fillRect(x + 2, y + 2, width * static_cast<uint32_t>(volume) / 100,
             height, UI_PURPLE);
}

void musicArtwork()
{
    roundedPanel(85, 36, 70, 64, UI_PURPLE);
    roundedPanel(87, 38, 66, 60, UI_PANEL);
    DrawMusicNote(99, 47, 3, UI_LAVENDER);
    DrawSparkle(58, 65, 4, UI_PINK);
    DrawSparkle(173, 49, 3, UI_PURPLE);
}

void branding(const char *status, uint16_t color)
{
    LCD_Fill(UI_BG);
    heading("MY PLAYER");
    musicArtwork();
    centered(119, "MP3 PLAYER", 3, UI_TEXT);
    centered(153, "RT-SPARK", 1, UI_TEXT_DIM);
    roundedPanel(24, 177, 192, 31, UI_PANEL);
    centered(185, status, 2, color);
    centered(224, "A LITTLE MUSIC MOMENT", 1, UI_TEXT_DIM);
}

void displayPlayback(bool paused, const Song &song, uint8_t songIndex,
                     uint8_t volume)
{
    LCD_Fill(UI_BG);
    heading("MY PLAYER");
    musicArtwork();
    char text[12];
    std::snprintf(text, sizeof(text), "SONG %u", songIndex + 1);
    LCD_DrawText(177, 88, text, UI_TEXT_DIM, 1);
    songText(song, 108, 134, 166);
    playbackLabels(paused);
    volumeNumber(volume);
    volumeBar(226, volume);
}

void largeVolumeNumber(uint8_t volume)
{
    fillRect(18, 105, 204, 47, UI_PANEL);
    char text[8];
    std::snprintf(text, sizeof(text), "%u", volume);
    const uint16_t numberWidth = std::strlen(text) * 36;
    const uint16_t x = (SCREEN_SIZE - numberWidth - 34) / 2;
    LCD_DrawText(x, 108, text, UI_TEXT, 6);
    DrawPercent(x + numberWidth + 6, 115, 2, UI_PINK);
}

void volumeFooter(const PlayerUiView &view)
{
    fillRect(MARGIN, 224, CONTENT_WIDTH, 9, UI_BG);
    char text[24];
    std::snprintf(text, sizeof(text), "%s - SONG %u",
                  view.playback == UiPlaybackState::Paused ? "PAUSED" : "PLAY",
                  view.currentIndex + 1);
    centered(225, text, 1, UI_TEXT_DIM);
}

void displayVolume(const PlayerUiView &view)
{
    LCD_Fill(UI_BG);
    DrawMusicNote(18, 6, 1, UI_PINK);
    DrawSparkle(209, 9, 3, UI_PURPLE);
    roundedPanel(12, 29, 216, 175, UI_PURPLE);
    roundedPanel(14, 31, 212, 171, UI_PANEL);
    centered(44, "VOLUME", 2, UI_LAVENDER);
    DrawSpeakerIcon(107, 68, 2, UI_PURPLE);
    largeVolumeNumber(view.volume);
    volumeBar(166, view.volume);
    centered(188, "TURN POT FOR VOLUME", 1, UI_TEXT_DIM);
    volumeFooter(view);
}

void bitBoxes(uint8_t bits, uint8_t previousBits = 0, bool partial = false)
{
    const char *labels[] = {"DOWN", "LEFT", "RIGHT"};
    for (uint8_t i = 0; i < 3; ++i)
    {
        if (partial && ((bits ^ previousBits) & (4 >> i)) == 0) continue;
        const uint16_t x = 46 + i * 52;
        const bool pressed = (bits & (4 >> i)) != 0;
        roundedPanel(x, 46, 44, 34, pressed ? UI_PURPLE : UI_MUTED);
        if (!pressed) roundedPanel(x + 1, 47, 42, 32, UI_PANEL);
        const char digit[] = {pressed ? '1' : '0', '\0'};
        LCD_DrawText(x + 14, 52, digit, pressed ? UI_BG : UI_LAVENDER, 3);
        LCD_DrawText(x + (44 - std::strlen(labels[i]) * 6) / 2, 84,
                     labels[i], UI_TEXT_DIM, 1);
    }
}

void countdown(uint8_t seconds)
{
    fillRect(18, 217, 204, 13, UI_PANEL);
    char text[12];
    std::snprintf(text, sizeof(text), "%u SEC", seconds);
    centered(217, text, 1, UI_PINK);
    fillRect(32, 229, 176, 3, UI_MUTED);
    fillRect(32, 229, 176UL * seconds / 5, 3, UI_PURPLE);
}

void selectionMetadata(const PlayerUiView &view)
{
    char text[16];
    std::snprintf(text, sizeof(text), "SONG %u", view.candidateIndex + 1);
    centered(103, text, 1, UI_LAVENDER);
    if (view.candidateSong) songText(*view.candidateSong, 119, 143, 176);
}

void selectionAction(Screen screen, uint8_t seconds)
{
    fillRect(MARGIN, 194, CONTENT_WIDTH, 40, UI_BG);
    roundedPanel(MARGIN, 194, CONTENT_WIDTH, 38, UI_PANEL);
    if (screen == Screen::Release)
    {
        centered(201, "RELEASE ALL", 2, UI_PINK);
        centered(221, "BUTTONS", 1, UI_TEXT_DIM);
    }
    else if (screen == Screen::Confirm)
    {
        centered(201, "UP CONFIRM", 2, UI_LAVENDER);
        countdown(seconds);
    }
    else
    {
        centered(201, "UP SELECT", 2, UI_LAVENDER);
        centered(221, "CHOOSE 000 - 111", 1, UI_TEXT_DIM);
    }
}

void displaySelection(const PlayerUiView &view, Screen screen, uint8_t seconds)
{
    LCD_Fill(UI_BG);
    selectionHeading(screen);
    bitBoxes(view.candidateIndex);
    selectionMetadata(view);
    selectionAction(screen, seconds);
}

void displayTimeout()
{
    LCD_Fill(UI_BG);
    heading("MY PLAYER");
    roundedPanel(79, 44, 82, 63, UI_PANEL);
    DrawMusicNote(97, 55, 3, UI_MUTED);
    for (uint8_t i = 0; i < 12; ++i)
    {
        fillRect(139 + i, 78 + i, 2, 2, UI_PINK);
        fillRect(150 - i, 78 + i, 2, 2, UI_PINK);
    }
    centered(128, "SONG CHANGE", 2, UI_TEXT);
    centered(155, "CANCELLED", 3, UI_PINK);
    centered(197, "NO SONG CHANGED", 1, UI_TEXT_DIM);
    centered(224, "RETURNING TO PLAYER", 1, UI_LAVENDER);
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
    volumeBar(226, volume, false);
    fillRect(148, 202, 80, 17, UI_BG);
    volumeNumber(volume);
}

void updatePlaybackLabels(bool paused)
{
    // Playing and paused share the artwork/title: redraw the status card only.
    fillRect(34, 180, 172, 24, UI_BG);
    playbackLabels(paused);
}
} // namespace

void PlayerUI_ShowStartup()
{
    PlayerUI_CancelTransient();
    renderedScreen = Screen::Startup;
    renderedSong = nullptr;
    renderedPlayback = UiPlaybackState::Idle;
    branding("INITIALIZING", UI_PINK);
}

void PlayerUI_ShowAudioReady(uint32_t now)
{
    startupStatusStart = now;
    startupStatus = renderedScreen = Screen::Ready;
    branding("AUDIO READY", UI_LAVENDER);
}

void PlayerUI_ShowAudioFailure(uint32_t now)
{
    startupStatusStart = now;
    startupStatus = renderedScreen = Screen::Failure;
    branding("INIT FAILED", UI_PINK);
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
            fillRect(MARGIN, 101, CONTENT_WIDTH, 85, UI_BG);
            selectionMetadata(view);
        }
        if (screen != renderedScreen)
        {
            selectionHeading(screen);
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
            volumeBar(166, view.volume, false);
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

#endif // PLAYER_UI_PURPLE_THEME
