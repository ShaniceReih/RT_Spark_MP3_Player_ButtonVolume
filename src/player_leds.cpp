#include "player_leds.h"
#include "stm32f4xx_hal.h"

namespace
{
constexpr uint16_t RED_PIN = GPIO_PIN_8;
constexpr uint16_t GREEN_PIN = GPIO_PIN_1;
constexpr uint16_t BLUE_PIN = GPIO_PIN_14;

bool initialized = false;
PlayerLedState appliedState = PLAYER_LED_IDLE;

void allOff()
{
    HAL_GPIO_WritePin(GPIOA, RED_PIN | GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, BLUE_PIN, GPIO_PIN_RESET);
}
}

void PlayerLeds_Init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Preload the output latches LOW before switching only the LED pin modes.
    allOff();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin = RED_PIN | GREEN_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = BLUE_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);
    appliedState = PLAYER_LED_IDLE;
    initialized = true;
}

void PlayerLeds_SetState(PlayerLedState state)
{
    // Invalid requests fail to all-off rather than leave a stale indication.
    switch (state)
    {
        case PLAYER_LED_IDLE:
        case PLAYER_LED_PLAYING:
        case PLAYER_LED_PAUSED:
        case PLAYER_LED_SELECTING:
            break;
        default:
            state = PLAYER_LED_IDLE;
            break;
    }

    if (!initialized) PlayerLeds_Init();
    if (state == appliedState) return;

    // Turn the previous color off before enabling its replacement.
    allOff();
    switch (state)
    {
        case PLAYER_LED_PLAYING:
            HAL_GPIO_WritePin(GPIOB, BLUE_PIN, GPIO_PIN_SET);
            break;
        case PLAYER_LED_PAUSED:
            HAL_GPIO_WritePin(GPIOA, RED_PIN, GPIO_PIN_SET);
            break;
        case PLAYER_LED_SELECTING:
            HAL_GPIO_WritePin(GPIOA, GREEN_PIN, GPIO_PIN_SET);
            break;
        case PLAYER_LED_IDLE:
        default:
            break;
    }
    appliedState = state;
}
