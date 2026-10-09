# Continuous PCM playback stage

This stage plays the complete eight existing note-table songs as synthesized PCM through circular I2S DMA. It does not read WAV or MP3 files. The project name and existing song-selection controls are retained; `platformio.ini` is unchanged.

The user physically verified this playback and the LCD/selection features before the PF6 volume integration. This guide now describes the current potentiometer controls. Button volume was an earlier development stage, preserved only in historical checkpoints; the new integrated board checks remain pending.

## Inspected baseline

- `src/main.cpp` owns UART logging, the ST7789V3 240x240 FSMC LCD, button polling, and idle/selecting/playing/paused UI states.
- `src/audio_codec.cpp` controls the ES8388 through software I2C and sends stereo PCM using SPI3/I2S3. The previous song demo used blocking `HAL_I2S_Transmit` calls and stopped after the first 20 events. Pause/resume previously changed the UI only.
- `include/song_def.h` holds periods in milliseconds, beats, tempos, and song objects; `include/song.h` defines `Song`. All note and beat array sizes match their declared lengths. Frequency is `1000 / periodMs`; zero is a rest. Duration preserves the existing `beat * 8 * tempo` seconds calculation.
- The archived working files establish the LCD, selection, pause, and volume behavior. `play_pause_working.cpp.txt` and `button_volume_base_working.cpp.txt` are identical; later checkpoints add the current enter/capture/confirm flow and real codec volume.

The following pin assignments include the retained baseline and the approved PF6 addition. The PF6 connection was physically tested by the user separately; these source assignments do not establish the new integrated board results:

| Function | Pins / setting |
| --- | --- |
| ES8388 software I2C | PF1 SCL, PF0 SDA; open-drain GPIO with pull-ups; 7-bit address 0x10 |
| I2S3 | PC7 MCLK, PA15 WS, PB3 CK, PB5 SD; AF6 SPI3 |
| Audio format | Philips I2S, master transmit, 16-bit stereo, nominal 44.1 kHz, MCLK enabled |
| Buttons, active low | UP PC5, DOWN PC1, LEFT PC0, RIGHT PC4 |
| Current potentiometer volume | PF6, ADC3 channel 4; 12-bit right-aligned; software polling, no ADC DMA |
| UART1 | PA9 TX, PA10 RX; 115200 baud |
| LCD | FSMC bank 3; PD3 reset and PF9 backlight |

## Behavior in this stage

A persistent stereo PCM buffer is transmitted by circular DMA. Half-transfer and transfer-complete callbacks refill the consumed half from the score, so LCD drawing and UART messages do not have to feed the stream. Notes and rests advance by sample frames. The full score replaces the 20-event demo limit.

SPI3 transmit uses DMA1 stream 5, channel 0, as documented in [ST's STM32F407 reference manual](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf). The 4096-byte buffer holds two halves of 512 stereo frames in ordinary SRAM. The existing clock, codec initialization sequence, and GPIO assignments are retained.

Volume is an ES8388 adjustment from 0 to 100, sourced from the PF6 potentiometer. ADC acquisition uses PCLK/4, a 480-cycle sample time and a bounded polling conversion at approximately the existing 20 ms loop interval. A four-sample moving average and two-percentage-point deadband reduce small jitter while allowing 0 and 100. One accepted logical value drives the codec and LCD; a failed ADC conversion preserves that value. LEFT/RIGHT button-volume events are ignored while their selection-bit handling remains intact. Changing volume does not restart the score. Pause mutes the codec and pauses DMA; resume continues from the retained buffer position and uses the latest potentiometer-selected volume. A confirmed replacement starts its score from the beginning, including when the previous song was paused. Natural completion is reported after the final buffered audio has been consumed.

Selection preserves the previous playing or paused state until a replacement is confirmed. Playback can reach its end while the selection screen is open; an expired selection then returns to idle rather than claiming the old song is still playing.

## Board checks

Build with `pio run -e black_f407zg`. Upload with the project's existing ST-Link setup when the board is ready. Observe UART1 at 115200 baud and the LCD.

1. Reset with the potentiometer near minimum. Expect PF6 / ADC3 channel 4 initialization, codec detection, audio initialization, the existing one-second startup test tone, and the idle screen. The codec initializes at logical zero, and any valid potentiometer-derived startup value is applied before that tone; there is no forced 50% setting. If startup ADC sampling fails, zero is retained until valid samples arrive. Successful audio logs include `AUDIO CODEC: ES8388 DETECTED!`, `AUDIO OUTPUT INITIALIZED!`, and `TEST TONE COMPLETE.`
2. For song 1, keep DOWN, LEFT, and RIGHT released (`000`). Short-press UP once to capture the candidate, release all buttons, then press UP again within five seconds to confirm. There is no initial enter-selection press. Expect `AUDIO: STARTING SONG (CONTINUOUS PCM/DMA)` and the playing screen. See `docs/song_selection_testing.md` for all eight combinations and input-conflict checks.
3. Let the score play beyond its first 20 events. Confirm that LCD updates and UART logging do not stop the stream. At the natural end, expect `AUDIO: SONG COMPLETE`, silence, and the idle screen.
4. During a song, rotate the potentiometer gradually. Confirm audible volume changes and matching accepted LCD/UART values without a restart. Check the 0 and 100 endpoints; keep the knob still and check that ADC jitter does not continually retrigger the overlay. With the knob fixed, LEFT and RIGHT taps must leave volume unchanged. See [potentiometer_volume_testing.md](potentiometer_volume_testing.md) for endpoint measurement and the complete test procedure.
5. Hold UP for at least one second, then release it. Confirm silence and the paused screen. Wait, change volume while paused, then hold and release UP again. Confirm continuation from the paused position.
6. While playing, hold the desired binary buttons and press UP to capture a candidate. Release all buttons, then wait more than five seconds without confirming. Expect timeout and restoration of the previous playback. Repeat while paused; it must remain paused. Repeat near the old song's end; if it finishes while selecting, timeout must return to idle.
7. While paused, hold different binary bits, press UP to capture the candidate, release all buttons, then press UP again within five seconds to confirm. Confirm the new song starts from its beginning and the screen shows playing.

The bits are DOWN = bit 2, LEFT = bit 1, RIGHT = bit 0:

| Bits | Song |
| --- | --- |
| 000 | Fur Elise |
| 001 | Canon in D |
| 010 | Minuet in G major |
| 011 | Turkish March |
| 100 | Nocturne in E flat |
| 101 | Waltz No. 2 |
| 110 | Nocturne in C sharp |
| 111 | Symphony No. 40 |

## Limits and next stage

Build and software checks cannot prove audible output, codec clock accuracy, analog levels, button bounce behavior, or physical pause/resume quality. Complete the board checks before treating those as verified. The startup test tone remains a blocking initialization check; song streaming uses DMA.

Potentiometer acquisition does not use DMA or change the audio DMA stream. The only volume-specific codec initialization change is the initial requested volume of zero. ADC acquisition and filtering are separate modules so they can later move into `adjust_volume()`; no FreeRTOS task, LCD mutex, LED control, USER_BUTTON change or sleep mode is implemented in this stage. The potentiometer addresses the laboratory's required volume input, but remaining laboratory items and new physical verification are still outstanding.

Transport or codec errors are reported on UART. If DMA cleanup itself fails, the driver disables output and requires a board reset/reinitialization before another playback attempt.

This is a melody synthesizer using the existing tables. File playback needs confirmed storage hardware, a storage/filesystem reader, and PCM WAV parsing first. MP3 playback additionally needs a decoder and enough buffering and processing capacity to keep DMA supplied. The available storage hardware and its wiring have not been confirmed.
