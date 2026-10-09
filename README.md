# RT-Spark Personal MP3 Player

A real-time embedded music player developed for the **RT-Spark STM32F407ZG** board using **FreeRTOS**, STM32Cube, PlatformIO, the ES8388 audio codec, onboard LCD, pushbuttons, potentiometer volume control, and external RGB LEDs.

The project supports **8 selectable songs**, real-time audio playback, binary song selection, pause/resume control, adjustable volume, LCD feedback, and RGB status indication.

---

## Features

- FreeRTOS-based embedded application
- 3 concurrent application tasks
- 8-song binary selection using pushbuttons
- 5-second song confirmation window
- Continuous PCM audio playback using DMA
- ES8388 audio codec
- Potentiometer-controlled volume
- Pause/resume playback
- 240×240 ST7789 LCD interface
- Purple/lavender music-player UI
- External RGB playback indicators
- Volume overlay
- Song selection and confirmation screens
- Runtime stack and heap monitoring
- Host-based unit and regression tests

---

## FreeRTOS Architecture

The application is divided into three main FreeRTOS tasks.

| Task | Priority | Stack | Responsibility |
|---|---:|---:|---|
| `PollButtonsTask` | 3 | 1024 words | Buttons, song selection, player state, playback transport and state publication |
| `AdjustVolumeTask` | 2 | 768 words | ADC sampling, filtering, accepted volume and codec volume updates |
| `UpdateLcdLedsTask` | 1 | 1024 words | LCD interface, PlayerUI and external RGB LED updates |

Audio playback itself remains DMA/interrupt-driven rather than using a fourth application task.

### Synchronization

The firmware uses separate synchronization objects for:

- player state
- ES8388 codec access
- LCD access

The UI task first copies a consistent player snapshot, releases the state mutex, and only then performs LCD rendering.

---

## System Architecture

```text
                     +----------------------+
                     |     FreeRTOS         |
                     +----------+-----------+
                                |
             +------------------+------------------+
             |                  |                  |
             v                  v                  v
   +------------------+ +------------------+ +----------------------+
   | PollButtonsTask  | | AdjustVolumeTask | | UpdateLcdLedsTask    |
   | Priority 3       | | Priority 2       | | Priority 1           |
   +--------+---------+ +--------+---------+ +----------+-----------+
            |                    |                      |
            v                    v                      v
       Pushbuttons          PF6 / ADC3              LCD + RGB LEDs
       Song control         Volume filter           Player interface
       Player state         ES8388 volume           Status display
            |
            v
      Audio transport
            |
            v
      I2S3 + DMA
            |
            v
         ES8388
            |
            v
       Audio output

Hardware
Main Board
- RT-Spark
- STM32F407ZGT6
- ARM Cortex-M4
- 240×240 ST7789 LCD
- ES8388 audio codec
Additional Components
- 10 kΩ potentiometer
- External RGB LEDs
- Current-limiting resistors
- Speaker / headphones
- Pushbutton controls
Pin Mapping
Pushbuttons
Control	STM32 Pin
UP	PC5
DOWN	PC1
LEFT	PC0
RIGHT	PC4


Potentiometer
Function	Pin
Volume input	PF6
ADC	ADC3 Channel 4


ADC resolution:
0 – 4095

External RGB LEDs
Color	STM32 Pin
Red	PA8
Green	PA1
Blue	PB14


LED behavior:
State	LED
Playing	Blue
Paused	Red
Selecting / Confirming	Green
Idle	Off


ES8388 Software I2C
Signal	Pin
SCL / SDA resources	PF0 / PF1


I2S3 Audio
Audio output uses I2S3 with DMA.
Relevant pins include:
- PA15
- PB3
- PB5
- PC7
Audio DMA:
DMA1 Stream5
Channel 0

Song Selection
Three buttons represent a 3-bit binary song number.
DOWN  = bit 2
LEFT  = bit 1
RIGHT = bit 0

Binary	Song
000	Fur Elise - Beethoven
001	Canon In D - Pachebelbel
010	Minuet in G major - Bach
011	Turkish March - Mozart
100	Nocturne in E flat - Chopin
101	Waltz No. 2 - Shostakovich
110	Nocturne in C sharp - Chopin
111	Symphony No. 40 - Mozart


Song Selection Procedure
1. Set the desired binary value using DOWN, LEFT and RIGHT.
2. Press UP to capture the selection.
3. Release all buttons.
4. Press UP again within 5 seconds.
5. The selected song begins playing.
Example:
LEFT + RIGHT
= 011
= Turkish March - Mozart

A timeout automatically cancels the selection if confirmation is not received within 5 seconds.
Playback Controls
Song Selection
DOWN  = bit 2
LEFT  = bit 1
RIGHT = bit 0
UP    = capture / confirm

Pause / Resume
Hold UP for approximately one second.
PLAYING -> long UP -> PAUSED
PAUSED  -> long UP -> PLAYING

Playback resumes from the previous position.
Volume Control
Volume is controlled using a potentiometer connected to:
PF6 -> ADC3 Channel 4

The volume system includes:
- 12-bit ADC sampling
- 4-sample filtering
- deadband protection
- accepted-volume tracking
- codec-safe volume updates
The physically observed potentiometer range is approximately:
8% – 100%

This range has intentionally not been recalibrated during the FreeRTOS migration.
LCD Interface
The project uses the onboard 240×240 ST7789 LCD.
The interface includes:
- startup screen
- ready screen
- binary song-selection display
- captured-song screen
- release-all prompt
- confirmation countdown
- now-playing screen
- paused screen
- timeout/cancellation notice
- volume overlay
Purple Music Player UI
A redesigned purple/lavender interface was added after the verified FreeRTOS baseline.
The UI includes:
- dark-purple background
- lavender accents
- music-note graphics
- play and pause icons
- speaker icon
- rounded visual panels
- binary selection boxes
- purple volume bar
- improved spacing and hierarchy
The redesigned UI is implemented in:
src/player_ui_purple.cpp

Firmware environment:
rtos_stage5_purple_ui

Purple UI resource usage:
Resource	Usage
Flash	47,344 bytes
RAM	48,392 bytes


The redesign increased Flash usage by approximately 1,288 bytes while RAM usage remained unchanged.
Audio Architecture
Songs are generated from synthesized musical-note data.
The project does not decode MP3 files.
Audio flow:
Song note / beat tables
        |
        v
PCM waveform generation
        |
        v
Stereo audio buffer
        |
        v
I2S3
        |
        v
DMA1 Stream5
        |
        v
ES8388 codec
        |
        v
Speaker / headphones

DMA operates continuously while the FreeRTOS application tasks handle controls, volume and presentation.
Building the Project
The project uses PlatformIO with STM32Cube.
Example PowerShell configuration:
$pio = 'C:\Users\tanqu\.platformio\penv\Scripts\platformio.exe'

Verified Stage-5 Firmware
& $pio run -e rtos_stage5_player

Upload:
& $pio run -e rtos_stage5_player -t upload

Serial monitor:
& $pio device monitor -e rtos_stage5_player --baud 115200

Purple UI Firmware
Build:
& $pio run -e rtos_stage5_purple_ui

Upload:
& $pio run -e rtos_stage5_purple_ui -t upload

Monitor:
& $pio device monitor -e rtos_stage5_purple_ui --baud 115200

Testing
Testing includes both host-side tests and physical-board verification.
The verified FreeRTOS system has been tested for:
- task startup
- all eight binary selections
- release gating
- 5-second confirmation timeout
- pause/resume
- volume while playing
- volume while paused
- song completion
- LCD updates
- RGB LED transitions
- state synchronization
- mutex behavior
- heap stability
- stack headroom
- rapid potentiometer changes
- rapid control interaction
- audio continuity
The purple PlayerUI also includes a dedicated host-test suite.
Verified Runtime Health
During Stage-5 physical testing, all three application tasks remained active with positive stack headroom and stable heap usage.
Example monitored values included approximately:
BUTTON_STACK ≈ 570 words free
VOLUME_STACK ≈ 715 words free
LCD_STACK    ≈ 670 words free

HEAP         ≈ 19,816 bytes
MIN_HEAP     ≈ 19,816 bytes

The stable minimum heap value showed no observed runtime memory leak during the test session.
Project Structure
RT_Spark_MP3_Player_ButtonVolume/
|
+-- include/
|   +-- application headers
|
+-- src/
|   +-- main.cpp
|   +-- audio implementation
|   +-- FreeRTOS task modules
|   +-- player state
|   +-- LCD / UI code
|   +-- player_ui.cpp
|   +-- player_ui_purple.cpp
|
+-- tests/
|   +-- host regression tests
|   +-- UI tests
|
+-- docs/
|   +-- FreeRTOS migration documentation
|   +-- hardware testing documentation
|   +-- purple UI testing
|
+-- scripts/
|
+-- platformio.ini
|
+-- README.md

Development History
The project was first developed as a working embedded music player and was then migrated incrementally to FreeRTOS.
Major development milestones:
Working embedded player
        |
        v
FreeRTOS kernel integration
        |
        v
Player moved under scheduler
        |
        v
Button/state task separation
        |
        v
Dedicated volume task
        |
        v
Dedicated LCD/LED task
        |
        v
Purple music-player UI

A verified Stage-5 checkpoint is preserved with the Git tag:
stage5-verified

Current Status
FreeRTOS integration       COMPLETE
Three application tasks    COMPLETE
8-song selection           COMPLETE
Audio playback             COMPLETE
Potentiometer volume       COMPLETE
LCD interface              COMPLETE
RGB LED indicators         COMPLETE
Purple PlayerUI            COMPLETE
Physical Stage-5 tests     PASSED

Pending Hardware Item
The laboratory documentation mentions a dedicated USER_BUTTON for stop/play behavior.
The current implementation uses long-UP for pause/resume.
A dedicated external USER_BUTTON implementation is therefore being kept as a possible final hardware-compliance addition if required.
Development Environment
- Visual Studio Code
- PlatformIO
- STM32Cube
- FreeRTOS
- GCC ARM Embedded Toolchain
- RT-Spark STM32F407ZG
Author
Developed as part of the BCA182 Embedded Systems Programming laboratory activities.