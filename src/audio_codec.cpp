#include "audio_codec.h"
#include "song.h"

#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
#include "codec_control.h"
#endif


/* =========================================================
   ES8388 CONTROL BUS
   RT-SPARK SOFTWARE I2C

   PF1 = SCL
   PF0 = SDA

   ES8388 address = 0x10
   ========================================================= */

#define AUDIO_SCL_PORT GPIOF
#define AUDIO_SCL_PIN  GPIO_PIN_1

#define AUDIO_SDA_PORT GPIOF
#define AUDIO_SDA_PIN  GPIO_PIN_0

#define ES8388_ADDR     0x10


/* =========================================================
   ES8388 REGISTERS
   ========================================================= */

#define ES8388_CONTROL1       0x00
#define ES8388_CONTROL2       0x01
#define ES8388_CHIPPOWER      0x02
#define ES8388_ADCPOWER       0x03
#define ES8388_DACPOWER       0x04
#define ES8388_MASTERMODE     0x08

#define ES8388_DACCONTROL1    0x17
#define ES8388_DACCONTROL2    0x18
#define ES8388_DACCONTROL3    0x19
#define ES8388_DACCONTROL4    0x1A
#define ES8388_DACCONTROL5    0x1B

#define ES8388_DACCONTROL16   0x26
#define ES8388_DACCONTROL17   0x27
#define ES8388_DACCONTROL20   0x2A
#define ES8388_DACCONTROL21   0x2B
#define ES8388_DACCONTROL23   0x2D
#define ES8388_DACCONTROL24   0x2E
#define ES8388_DACCONTROL25   0x2F


/* =========================================================
   I2S
   ========================================================= */

static I2S_HandleTypeDef hi2s3;
static DMA_HandleTypeDef hdmaI2s3Tx;
static bool audioReady = false;

// Ordinary SRAM (.bss), not CCM: DMA1 must be able to read this buffer.
static const uint32_t AUDIO_SAMPLE_RATE = 44100;
static const uint32_t FRAMES_PER_HALF = 512;
static const uint32_t WORDS_PER_HALF = FRAMES_PER_HALF * 2;
alignas(4) static int16_t audioBuffer[WORDS_PER_HALF * 2];

static volatile AudioPlaybackState playbackState = AUDIO_STOPPED;
static volatile bool streamActive = false;
static const Song *playingSong = nullptr;
static uint32_t nextNote = 0;
static uint32_t noteFramesRemaining = 0;
static uint32_t gapFramesRemaining = 0;
static uint32_t phase = 0;
static uint32_t phaseStep = 0;
static bool noteHasGap = false;
static bool sourceFinished = false;
static bool halfEndsSong[2] = {false, false};


/* =========================================================
   SOFTWARE I2C
   ========================================================= */

static void I2C_Delay(void)
{
    for (
        volatile uint32_t i = 0;
        i < 120;
        i++
    )
    {
        __NOP();
    }
}


static void SDA_High(void)
{
    HAL_GPIO_WritePin(
        AUDIO_SDA_PORT,
        AUDIO_SDA_PIN,
        GPIO_PIN_SET
    );
}


static void SDA_Low(void)
{
    HAL_GPIO_WritePin(
        AUDIO_SDA_PORT,
        AUDIO_SDA_PIN,
        GPIO_PIN_RESET
    );
}


static void SCL_High(void)
{
    HAL_GPIO_WritePin(
        AUDIO_SCL_PORT,
        AUDIO_SCL_PIN,
        GPIO_PIN_SET
    );
}


static void SCL_Low(void)
{
    HAL_GPIO_WritePin(
        AUDIO_SCL_PORT,
        AUDIO_SCL_PIN,
        GPIO_PIN_RESET
    );
}


static void AudioI2C_Init(void)
{
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin =
        AUDIO_SCL_PIN |
        AUDIO_SDA_PIN;

    gpio.Mode =
        GPIO_MODE_OUTPUT_OD;

    gpio.Pull =
        GPIO_PULLUP;

    gpio.Speed =
        GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOF,
        &gpio
    );

    SDA_High();
    SCL_High();

    I2C_Delay();
}


static void I2C_Start(void)
{
    SDA_High();
    SCL_High();

    I2C_Delay();

    SDA_Low();

    I2C_Delay();

    SCL_Low();

    I2C_Delay();
}


static void I2C_Stop(void)
{
    SDA_Low();

    I2C_Delay();

    SCL_High();

    I2C_Delay();

    SDA_High();

    I2C_Delay();
}


static bool I2C_WriteByte(
    uint8_t value)
{
    for (
        uint8_t bit = 0;
        bit < 8;
        bit++
    )
    {
        if (value & 0x80)
        {
            SDA_High();
        }
        else
        {
            SDA_Low();
        }

        I2C_Delay();

        SCL_High();

        I2C_Delay();

        SCL_Low();

        I2C_Delay();

        value <<= 1;
    }


    /* Release SDA for ACK */

    SDA_High();

    I2C_Delay();

    SCL_High();

    I2C_Delay();


    GPIO_PinState ack =
        HAL_GPIO_ReadPin(
            AUDIO_SDA_PORT,
            AUDIO_SDA_PIN
        );


    SCL_Low();

    I2C_Delay();


    return (
        ack ==
        GPIO_PIN_RESET
    );
}


/* =========================================================
   WRITE ES8388 REGISTER
   ========================================================= */

static bool ES8388_Write(
    uint8_t reg,
    uint8_t value)
{
    I2C_Start();

    if (
        !I2C_WriteByte(
            ES8388_ADDR << 1
        )
    )
    {
        I2C_Stop();
        return false;
    }


    if (
        !I2C_WriteByte(reg)
    )
    {
        I2C_Stop();
        return false;
    }


    if (
        !I2C_WriteByte(value)
    )
    {
        I2C_Stop();
        return false;
    }


    I2C_Stop();

    return true;
}


/* =========================================================
   PROBE
   ========================================================= */

bool AudioCodec_Probe(void)
{
    AudioI2C_Init();

    I2C_Start();

    bool detected =
        I2C_WriteByte(
            ES8388_ADDR << 1
        );

    I2C_Stop();

    return detected;
}


/* =========================================================
   I2S GPIO INITIALIZATION

   Official RT-Spark audio wiring:

   PC7  = I2S3 MCLK
   PA15 = I2S3 WS
   PB3  = I2S3 CK
   PB5  = I2S3 SD
   ========================================================= */

extern "C"
void HAL_I2S_MspInit(
    I2S_HandleTypeDef *handle)
{
    if (
        handle->Instance != SPI3
    )
    {
        return;
    }


    __HAL_RCC_SPI3_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();


    GPIO_InitTypeDef gpio = {0};


    /* PC7 = MCLK */

    gpio.Pin =
        GPIO_PIN_7;

    gpio.Mode =
        GPIO_MODE_AF_PP;

    gpio.Pull =
        GPIO_NOPULL;

    gpio.Speed =
        GPIO_SPEED_FREQ_VERY_HIGH;

    gpio.Alternate =
        GPIO_AF6_SPI3;

    HAL_GPIO_Init(
        GPIOC,
        &gpio
    );


    /* PA15 = WS */

    gpio.Pin =
        GPIO_PIN_15;

    HAL_GPIO_Init(
        GPIOA,
        &gpio
    );


    /* PB3 = CK
       PB5 = SD */

    gpio.Pin =
        GPIO_PIN_3 |
        GPIO_PIN_5;

    HAL_GPIO_Init(
        GPIOB,
        &gpio
    );
}


/* =========================================================
   I2S INITIALIZATION
   ========================================================= */

static bool AudioI2S_Init(void)
{
    RCC_PeriphCLKInitTypeDef clockConfig = {0};

    clockConfig.PeriphClockSelection =
        RCC_PERIPHCLK_I2S;

    clockConfig.PLLI2S.PLLI2SN =
        192;

    clockConfig.PLLI2S.PLLI2SR =
        2;


    if (
        HAL_RCCEx_PeriphCLKConfig(
            &clockConfig
        ) != HAL_OK
    )
    {
        return false;
    }


    hi2s3.Instance =
        SPI3;

    hi2s3.Init.Mode =
        I2S_MODE_MASTER_TX;

    hi2s3.Init.Standard =
        I2S_STANDARD_PHILIPS;

    hi2s3.Init.DataFormat =
        I2S_DATAFORMAT_16B;

    hi2s3.Init.MCLKOutput =
        I2S_MCLKOUTPUT_ENABLE;

    hi2s3.Init.AudioFreq =
        I2S_AUDIOFREQ_44K;

    hi2s3.Init.CPOL =
        I2S_CPOL_LOW;

    hi2s3.Init.ClockSource =
        I2S_CLOCK_PLL;

    hi2s3.Init.FullDuplexMode =
        I2S_FULLDUPLEXMODE_DISABLE;


    if (
        HAL_I2S_Init(
            &hi2s3
        ) != HAL_OK
    )
    {
        return false;
    }


    return true;
}

/* =========================================================
   CIRCULAR DMA TRANSPORT

   SPI3 TX = DMA1, stream 5, channel 0 (STM32F407 RM0090).
   Each half contains 512 stereo frames, approximately 11.6 ms.
   ========================================================= */

static bool AudioDMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdmaI2s3Tx.Instance = DMA1_Stream5;
    hdmaI2s3Tx.Init.Channel = DMA_CHANNEL_0;
    hdmaI2s3Tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdmaI2s3Tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdmaI2s3Tx.Init.MemInc = DMA_MINC_ENABLE;
    hdmaI2s3Tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdmaI2s3Tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdmaI2s3Tx.Init.Mode = DMA_CIRCULAR;
    hdmaI2s3Tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdmaI2s3Tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdmaI2s3Tx) != HAL_OK)
    {
        return false;
    }

    __HAL_LINKDMA(&hi2s3, hdmatx, hdmaI2s3Tx);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
    // Enabled only after a complete buffer has been prepared.
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    return true;
}

static bool Audio_SetMuted(bool muted)
{
#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
    // Serialize only the control-bus transaction, never PCM refill/transport.
    CodecControl_Lock();
    const bool written = ES8388_Write(ES8388_DACCONTROL3, muted ? 0x04 : 0x00);
    CodecControl_Unlock();
    return written;
#else
    return ES8388_Write(ES8388_DACCONTROL3, muted ? 0x04 : 0x00);
#endif
}

// This source does not read files or decode MP3. It preserves the existing
// note periods, beat timing, square wave amplitude and 20 ms note gaps.
static int16_t Audio_NextSample(void)
{
    if (sourceFinished)
    {
        return 0;
    }

    if (gapFramesRemaining != 0)
    {
        --gapFramesRemaining;
        if (gapFramesRemaining == 0 && nextNote >= (uint32_t)playingSong->length)
        {
            sourceFinished = true;
        }
        return 0;
    }

    if (noteFramesRemaining == 0)
    {
        if (nextNote >= (uint32_t)playingSong->length)
        {
            sourceFinished = true;
            return 0;
        }

        float periodMs = playingSong->note[nextNote];
        uint32_t durationMs = (uint32_t)(playingSong->beat[nextNote] *
                                       8.0f * playingSong->tempo * 1000.0f);
        if (durationMs < 20)
        {
            durationMs = 20;
        }

        noteFramesRemaining = (durationMs * AUDIO_SAMPLE_RATE) / 1000;
        noteHasGap = periodMs > 0.0f;
        phaseStep = noteHasGap
            ? (uint32_t)((1000.0f / periodMs) *
                         (4294967296.0f / AUDIO_SAMPLE_RATE))
            : 0;
        phase = 0;
        ++nextNote;
    }

    int16_t sample = 0;
    if (noteHasGap)
    {
        phase += phaseStep;
        sample = (phase & 0x80000000UL) ? 4500 : -4500;
    }

    --noteFramesRemaining;
    if (noteFramesRemaining == 0)
    {
        if (noteHasGap)
        {
            gapFramesRemaining = AUDIO_SAMPLE_RATE / 50; // 20 ms
        }
        else if (nextNote >= (uint32_t)playingSong->length)
        {
            sourceFinished = true;
        }
    }
    return sample;
}

static void Audio_FillHalf(uint32_t half)
{
    int16_t *destination = &audioBuffer[half * WORDS_PER_HALF];
    halfEndsSong[half] = false;

    for (uint32_t frame = 0; frame < FRAMES_PER_HALF; ++frame)
    {
        bool wasFinished = sourceFinished;
        int16_t sample = Audio_NextSample();
        destination[frame * 2] = sample;
        destination[frame * 2 + 1] = sample;
        if (!wasFinished && sourceFinished)
        {
            halfEndsSong[half] = true;
        }
    }
    __DMB();
}

static void Audio_HalfConsumed(uint32_t half)
{
    if (!streamActive || playbackState == AUDIO_FINISHED ||
        playbackState == AUDIO_ERROR)
    {
        return;
    }

    if (halfEndsSong[half])
    {
        // EOF means the final half was transmitted, not merely generated.
        // Clear it so the circular stream remains silent until main stops it.
        for (uint32_t word = 0; word < WORDS_PER_HALF; ++word)
        {
            audioBuffer[half * WORDS_PER_HALF + word] = 0;
        }
        __DMB();
        playbackState = AUDIO_FINISHED;
        return;
    }

    // Bounded synthesis only. Never print, write I2C, allocate or delay here.
    // Refilling in the IRQ avoids underruns during foreground LCD/UART work.
    Audio_FillHalf(half);
}

extern "C" void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdmaI2s3Tx);
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &hi2s3)
    {
        Audio_HalfConsumed(0);
    }
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &hi2s3)
    {
        Audio_HalfConsumed(1);
    }
}

extern "C" void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *handle)
{
    if (handle == &hi2s3 && streamActive)
    {
        // Stop requests immediately; blocking mute/cleanup belongs to main.
        CLEAR_BIT(hi2s3.Instance->CR2, SPI_CR2_TXDMAEN);
        playbackState = AUDIO_ERROR;
    }
}

static bool Audio_StopTransport(void)
{
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    streamActive = false;
    bool stopped = HAL_I2S_DMAStop(&hi2s3) == HAL_OK;
    // Also leave output disabled if HAL reported an abort/timeout failure.
    CLEAR_BIT(hi2s3.Instance->CR2, SPI_CR2_TXDMAEN);
    __HAL_I2S_DISABLE(&hi2s3);
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
    playingSong = nullptr;
    if (!stopped)
    {
        audioReady = false; // Reinitialize/reset before attempting playback.
        playbackState = AUDIO_ERROR;
    }
    return stopped;
}

void Audio_StopSong(void)
{
    if (streamActive)
    {
        Audio_SetMuted(true);
        if (!Audio_StopTransport())
        {
            return;
        }
    }
    playbackState = AUDIO_STOPPED;
}

bool Audio_StartSong(const Song *song)
{
    Audio_StopSong();
    if (!audioReady || song == nullptr || song->length <= 0 ||
        song->note == nullptr || song->beat == nullptr ||
        !Audio_SetMuted(true))
    {
        playbackState = AUDIO_ERROR;
        return false;
    }

    playingSong = song;
    nextNote = 0;
    noteFramesRemaining = 0;
    gapFramesRemaining = 0;
    sourceFinished = false;
    Audio_FillHalf(0);
    Audio_FillHalf(1);

    playbackState = AUDIO_PLAYING;
    streamActive = true;
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    if (HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t *)audioBuffer,
                           WORDS_PER_HALF * 2) != HAL_OK ||
        !Audio_SetMuted(false) || playbackState == AUDIO_ERROR)
    {
        Audio_SetMuted(true);
        Audio_StopTransport();
        playbackState = AUDIO_ERROR;
        return false;
    }
    return true;
}

bool Audio_PauseSong(void)
{
    if (playbackState != AUDIO_PLAYING || !Audio_SetMuted(true))
    {
        return false;
    }

    // The final half can finish while the foreground I2C write runs.
    HAL_NVIC_DisableIRQ(DMA1_Stream5_IRQn);
    bool paused = playbackState == AUDIO_PLAYING &&
                  HAL_I2S_DMAPause(&hi2s3) == HAL_OK;
    if (paused)
    {
        playbackState = AUDIO_PAUSED;
    }
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    if (!paused && playbackState == AUDIO_PLAYING)
    {
        Audio_SetMuted(false);
    }
    return paused;
}

bool Audio_ResumeSong(void)
{
    if (playbackState != AUDIO_PAUSED)
    {
        return false;
    }

    // DMA keeps its buffer position; the melody does not restart.
    playbackState = AUDIO_PLAYING;
    if (HAL_I2S_DMAResume(&hi2s3) != HAL_OK || !Audio_SetMuted(false) ||
        playbackState == AUDIO_ERROR)
    {
        Audio_SetMuted(true);
        Audio_StopTransport();
        playbackState = AUDIO_ERROR;
        return false;
    }
    return true;
}

void Audio_Service(void)
{
    if (streamActive && (playbackState == AUDIO_FINISHED ||
                         playbackState == AUDIO_ERROR))
    {
        if (!Audio_SetMuted(true))
        {
            playbackState = AUDIO_ERROR;
        }
        Audio_StopTransport();
    }
}

AudioPlaybackState Audio_GetPlaybackState(void)
{
    return playbackState;
}


/* =========================================================
   REAL CODEC VOLUME

   0   = quiet
   100 = maximum
   ========================================================= */

void AudioCodec_SetVolume(
    uint8_t volume)
{
    if (
        volume > 100
    )
    {
        volume = 100;
    }


    /*
     * Same mapping used by the RT-Spark ES8388 driver.
     */

    uint8_t attenuation =
        (uint8_t)(
            (
                192UL *
                (100UL - volume)
            )
            / 100UL
        );


    ES8388_Write(
        ES8388_DACCONTROL4,
        attenuation
    );

    ES8388_Write(
        ES8388_DACCONTROL5,
        attenuation
    );
}


/* =========================================================
   CODEC INITIALIZATION
   ========================================================= */

bool AudioCodec_Init(void)
{
    audioReady = false;
    AudioI2C_Init();


    if (
        !AudioCodec_Probe()
    )
    {
        return false;
    }


    /*
     * Start muted.
     */

    if (
        !ES8388_Write(
            ES8388_DACCONTROL3,
            0x04
        )
    )
    {
        return false;
    }


    ES8388_Write(
        ES8388_CONTROL2,
        0x50
    );


    ES8388_Write(
        ES8388_CHIPPOWER,
        0x00
    );


    /*
     * Codec operates as I2S slave.
     */

    ES8388_Write(
        ES8388_MASTERMODE,
        0x00
    );


    /*
     * Configure DAC.
     */

    ES8388_Write(
        ES8388_DACPOWER,
        0xC0
    );


    ES8388_Write(
        ES8388_CONTROL1,
        0x12
    );


    /*
     * 16-bit I2S.
     */

    ES8388_Write(
        ES8388_DACCONTROL1,
        0x18
    );


    ES8388_Write(
        ES8388_DACCONTROL2,
        0x02
    );


    ES8388_Write(
        ES8388_DACCONTROL16,
        0x00
    );


    ES8388_Write(
        ES8388_DACCONTROL17,
        0x9C
    );


    ES8388_Write(
        ES8388_DACCONTROL20,
        0x9C
    );


    ES8388_Write(
        ES8388_DACCONTROL21,
        0x80
    );


    ES8388_Write(
        ES8388_DACCONTROL23,
        0x00
    );


    /*
     * Enable DAC / headphone outputs.
     */

    ES8388_Write(
        ES8388_DACPOWER,
        0x3C
    );


    /*
     * LOUT1 / ROUT1 output levels.
     */

    ES8388_Write(
        ES8388_DACCONTROL24,
        0x1E
    );


    ES8388_Write(
        ES8388_DACCONTROL25,
        0x1E
    );


    /*
     * Keep logical volume zero until initialization is complete. The caller
     * applies the accepted potentiometer setting before starting audio.
     */

    AudioCodec_SetVolume(
        0
    );


    /*
     * Restart codec state machine.
     */

    ES8388_Write(
        ES8388_CHIPPOWER,
        0xF0
    );

    HAL_Delay(10);

    ES8388_Write(
        ES8388_CHIPPOWER,
        0x00
    );


    /*
     * Unmute DAC.
     */

    ES8388_Write(
        ES8388_DACCONTROL3,
        0x00
    );


    if (
        !AudioI2S_Init()
    )
    {
        return false;
    }

    if (!AudioDMA_Init())
    {
        return false;
    }
    audioReady = true;
    return true;
}


/* =========================================================
   TEST TONE

   Generates a quiet ~440 Hz square wave.

   Duration is specified in milliseconds.
   ========================================================= */

bool Audio_PlayTestTone(
    uint32_t durationMs)
{
    /*
     * 128 stereo frames:
     *
     * L, R, L, R...
     */

    int16_t buffer[256];


    uint32_t phase =
        0;


    /*
     * 440 Hz phase increment
     * at approximately 44.1 kHz.
     */

    const uint32_t phaseStep =
        42852281UL;


    uint32_t start =
        HAL_GetTick();


    while (
        HAL_GetTick() -
        start <
        durationMs
    )
    {
        for (
            uint32_t frame = 0;
            frame < 128;
            frame++
        )
        {
            phase +=
                phaseStep;


            int16_t sample;


            if (
                phase &
                0x80000000UL
            )
            {
                sample =
                    5000;
            }
            else
            {
                sample =
                    -5000;
            }


            /*
             * Stereo:
             * same sample to LEFT and RIGHT.
             */

            buffer[
                frame * 2
            ] =
                sample;


            buffer[
                frame * 2 + 1
            ] =
                sample;
        }


        if (
            HAL_I2S_Transmit(
                &hi2s3,
                (uint16_t *)buffer,
                256,
                100
            ) != HAL_OK
        )
        {
            return false;
        }
    }


    /*
     * End with silence.
     */

    for (
        uint32_t i = 0;
        i < 256;
        i++
    )
    {
        buffer[i] =
            0;
    }


    HAL_I2S_Transmit(
        &hi2s3,
        (uint16_t *)buffer,
        256,
        100
    );


    return true;
}

void Audio_Silence(uint32_t durationMs)
{
    int16_t buffer[256] = {0};

    uint32_t start =
        HAL_GetTick();

    while (
        HAL_GetTick() - start <
        durationMs
    )
    {
        HAL_I2S_Transmit(
            &hi2s3,
            (uint16_t *)buffer,
            256,
            100
        );
    }
}


void Audio_PlayNote(
    float frequencyHz,
    uint32_t durationMs)
{
    if (
        frequencyHz <= 0.0f
    )
    {
        Audio_Silence(
            durationMs
        );

        return;
    }


    int16_t buffer[256];

    uint32_t phase =
        0;


    /*
     * I2S is approximately 44.1 kHz.
     */

    uint32_t phaseStep =
        (uint32_t)(
            frequencyHz *
            (
                4294967296.0f /
                44100.0f
            )
        );


    uint32_t start =
        HAL_GetTick();


    while (
        HAL_GetTick() - start <
        durationMs
    )
    {
        for (
            uint32_t frame = 0;
            frame < 128;
            frame++
        )
        {
            phase +=
                phaseStep;


            int16_t sample =
                (
                    phase &
                    0x80000000UL
                )
                ? 4500
                : -4500;


            buffer[
                frame * 2
            ] =
                sample;


            buffer[
                frame * 2 + 1
            ] =
                sample;
        }


        HAL_I2S_Transmit(
            &hi2s3,
            (uint16_t *)buffer,
            256,
            100
        );
    }


    /*
     * Tiny gap between notes.
     */

    Audio_Silence(
        20
    );
}
