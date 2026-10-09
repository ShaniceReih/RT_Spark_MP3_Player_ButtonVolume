#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
#include "../src/audio_codec.cpp"
#include "../include/song_def.h"

// These tests include the real driver; the stub controls only HAL transport.
static void resetHost() {
    hostAck = true;
    hostStartResult = hostPauseResult = hostResumeResult = hostStopResult = HAL_OK;
    hostOnGpioWrite = nullptr;
    Audio_StopSong();
    assert(!hostTransportRunning);
}

static void callback(unsigned half) {
    const unsigned gpioBefore = hostGpioCalls;
    const unsigned stopBefore = hostStopCalls;
    hostInIrq = true;
    if (half == 0) HAL_I2S_TxHalfCpltCallback(&hi2s3);
    else HAL_I2S_TxCpltCallback(&hi2s3);
    hostInIrq = false;
    assert(hostGpioCalls == gpioBefore && hostStopCalls == stopBefore);
}

static std::vector<int16_t> consumeSong(unsigned expectedHalves) {
    std::vector<int16_t> frames;
    for (unsigned index = 0; index < expectedHalves; ++index) {
        assert(Audio_GetPlaybackState() == AUDIO_PLAYING);
        assert(hostTransportRunning && hostRequestsEnabled && hostIrqEnabled);
        unsigned half = index % 2;
        for (unsigned frame = 0; frame < 512; ++frame) {
            unsigned word = half * 1024 + frame * 2;
            assert(hostMemory[word] == hostMemory[word + 1]);
            frames.push_back(hostMemory[word]);
        }
        hostNdtr = half == 0 ? 1024 : 2048;
        callback(half);
        assert(Audio_GetPlaybackState() ==
               (index + 1 == expectedHalves ? AUDIO_FINISHED : AUDIO_PLAYING));
    }
    assert(hostTransportRunning); // EOF cleanup must remain in foreground.
    return frames;
}

static void errorInterrupt() {
    hostInIrq = true;
    HAL_I2S_ErrorCallback(&hi2s3);
    hostInIrq = false;
}

static void testTailAndSignal() {
    float note[] = {1000.0f / 440.0f};
    float beat[] = {0.01f}; // Clamped to 20 ms.
    Song tone("tone", "", note, beat, 0.25f, 1);
    assert(Audio_StartSong(&tone));
    assert(hostTransferWords == 2048);
    auto output = consumeSong(4); // 882 tone frames + 882 gap frames.
    for (unsigned i = 0; i < 882; ++i) assert(output[i] == -4500 || output[i] == 4500);
    assert(std::any_of(output.begin(), output.begin() + 882, [](int16_t v) { return v > 0; }));
    assert(std::any_of(output.begin(), output.begin() + 882, [](int16_t v) { return v < 0; }));
    for (unsigned i = 882; i < output.size(); ++i) assert(output[i] == 0);
    Audio_Service();
    assert(!hostTransportRunning && !hostIrqEnabled);
    assert(Audio_GetPlaybackState() == AUDIO_FINISHED);

    float rest[] = {0};
    Song shortRest("rest", "", rest, beat, 0.25f, 1);
    assert(Audio_StartSong(&shortRest));
    output = consumeSong(2); // 512 frames + a 370-frame tail; no note gap.
    assert(std::all_of(output.begin(), output.end(), [](int16_t v) { return v == 0; }));
    Audio_Service();
}

static void testExactBoundaries() {
    float rest[] = {0};
    float exactHalf[] = {0.64f};
    Song half("half", "", rest, exactHalf, 1.0f, 1);
    assert(Audio_StartSong(&half));
    consumeSong(441); // 5120 ms => 225792 frames => exactly 441 halves.
    Audio_Service();
    float exactRing[] = {1.28f};
    Song ring("ring", "", rest, exactRing, 1.0f, 1);
    assert(Audio_StartSong(&ring));
    consumeSong(882); // 10240 ms => exactly 441 complete buffers.
    Audio_Service();
}

static void testPauseAndRestart() {
    assert(Audio_StartSong(&FUR_ELISE));
    hostNdtr = 1638;
    auto savedBuffer = std::vector<int16_t>(hostMemory, hostMemory + 2048);
    unsigned savedNote = nextNote, savedNoteFrames = noteFramesRemaining;
    uint32_t savedPhase = phase;
    assert(Audio_PauseSong());
    assert(Audio_GetPlaybackState() == AUDIO_PAUSED && !hostRequestsEnabled);
    assert(hostNdtr == 1638 && nextNote == savedNote && noteFramesRemaining == savedNoteFrames);
    assert(phase == savedPhase && std::equal(savedBuffer.begin(), savedBuffer.end(), hostMemory));
    assert(!Audio_PauseSong()); // Repeated pause does not move cursor.
    assert(Audio_ResumeSong());
    assert(Audio_GetPlaybackState() == AUDIO_PLAYING && hostRequestsEnabled);
    assert(hostNdtr == 1638 && nextNote == savedNote && phase == savedPhase);
    assert(std::equal(savedBuffer.begin(), savedBuffer.end(), hostMemory));
    assert(!Audio_ResumeSong());
    assert(Audio_PauseSong());
    unsigned stoppedBefore = hostStopCalls;
    assert(Audio_StartSong(&CANNON_IN_D));
    assert(hostStopCalls == stoppedBefore + 1);
    assert(hostNdtr == 2048 && playingSong == &CANNON_IN_D);
    Audio_StopSong();
    assert(Audio_GetPlaybackState() == AUDIO_STOPPED && !hostIrqEnabled);
    callback(0); callback(1); // Late notifications must not dereference stopped song.
    assert(Audio_GetPlaybackState() == AUDIO_STOPPED);
}

static void testFailures() {
    assert(!Audio_StartSong(nullptr));
    assert(Audio_GetPlaybackState() == AUDIO_ERROR && !hostTransportRunning);
    resetHost();

    hostStartResult = HAL_ERROR;
    assert(!Audio_StartSong(&FUR_ELISE));
    assert(Audio_GetPlaybackState() == AUDIO_ERROR && !hostTransportRunning && !hostIrqEnabled);
    resetHost();

    hostAck = false; // Codec control bus NACK.
    assert(!Audio_StartSong(&FUR_ELISE));
    assert(Audio_GetPlaybackState() == AUDIO_ERROR && !hostTransportRunning);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    errorInterrupt();
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    assert((hi2s3.Instance->CR2 & SPI_CR2_TXDMAEN) == 0);
    assert(hostTransportRunning); // Blocking cleanup not allowed in error IRQ.
    Audio_Service();
    assert(!hostTransportRunning && !hostIrqEnabled);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    hostPauseResult = HAL_ERROR;
    assert(!Audio_PauseSong());
    assert(Audio_GetPlaybackState() == AUDIO_PLAYING);
    hostPauseResult = HAL_OK;
    assert(Audio_PauseSong());
    hostResumeResult = HAL_ERROR;
    assert(!Audio_ResumeSong());
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    Audio_Service();
    assert(!hostTransportRunning);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    assert(Audio_PauseSong());
    hostAck = false; // Resume unmute fails after transport resumes.
    assert(!Audio_ResumeSong());
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    Audio_Service();
    assert(!hostTransportRunning && !hostIrqEnabled);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    hostOnGpioWrite = errorInterrupt; // Fault arriving during foreground mute.
    assert(!Audio_PauseSong());
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    Audio_Service();
    assert(!hostTransportRunning);
    resetHost();
}

static void testFullFurElise() {
    // Independent score-duration calculation, without reproducing the PCM renderer.
    uint32_t expectedFrames = 0;
    assert(FUR_ELISE.length == 72);
    for (int note = 0; note < FUR_ELISE.length; ++note) {
        uint32_t milliseconds = static_cast<uint32_t>(
            FUR_ELISE.beat[note] * 8.0f * FUR_ELISE.tempo * 1000.0f);
        milliseconds = std::max<uint32_t>(20, milliseconds);
        expectedFrames += milliseconds * 44100 / 1000;
        if (FUR_ELISE.note[note] > 0) expectedFrames += 882;
    }
    assert(Audio_StartSong(&FUR_ELISE));
    consumeSong((expectedFrames + 511) / 512);
    assert(nextNote == 72);
    Audio_Service();
    assert(!hostTransportRunning);
    std::cout << "Full Fur Elise: 72 score events, " << expectedFrames << " PCM frames\n";
}

static void errorWhenActive() {
    if (streamActive) errorInterrupt();
    else hostOnGpioWrite = errorWhenActive;
}

static void testUnmuteAndCleanupFailures() {
    resetHost();
    hostOnGpioWrite = errorWhenActive;
    assert(!Audio_StartSong(&FUR_ELISE)); // DMA error during start's unmute I2C.
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    assert(!hostTransportRunning && !hostIrqEnabled);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    assert(Audio_PauseSong());
    hostOnGpioWrite = errorInterrupt; // DMA error during resume's unmute I2C.
    assert(!Audio_ResumeSong());
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    assert(!hostTransportRunning && !hostIrqEnabled);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    errorInterrupt();
    hostAck = false; // Foreground error cleanup cannot mute the codec.
    Audio_Service();
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    assert(!hostTransportRunning && !hostIrqEnabled);
    resetHost();

    float rest[] = {0}, beat[] = {0.01f};
    Song finalRest("rest", "", rest, beat, 0.25f, 1);
    assert(Audio_StartSong(&finalRest));
    consumeSong(2);
    hostAck = false; // Failed mute changes EOF cleanup from finished to error.
    Audio_Service();
    assert(Audio_GetPlaybackState() == AUDIO_ERROR && !hostTransportRunning);
    resetHost();

    assert(Audio_StartSong(&FUR_ELISE));
    hostStopResult = HAL_ERROR; // Simulate DMA abort/BSY timeout failure.
    Audio_StopSong();
    assert(Audio_GetPlaybackState() == AUDIO_ERROR);
    assert(!hostIrqEnabled && !hostI2sEnabled && !audioReady);
    assert((hi2s3.Instance->CR2 & SPI_CR2_TXDMAEN) == 0);
    hostStopResult = HAL_OK;
    assert(!Audio_StartSong(&FUR_ELISE)); // Transport failure requires recovery.
    hostTransportRunning = hostRequestsEnabled = false; // Simulated board reset.
    assert(AudioCodec_Init());
    resetHost();
}
int main() {
    assert(AudioCodec_Init());
    testTailAndSignal();
    testExactBoundaries();
    testPauseAndRestart();
    testFailures();
    testFullFurElise();
    testUnmuteAndCleanupFailures();
    std::cout << "PASS: real DMA engine PCM, short EOF tail, exact half/ring EOF, pause cursor, restart, IRQ guards, and failure cleanup/unmute races\n";
}


