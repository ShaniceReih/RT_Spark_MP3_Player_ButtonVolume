#ifndef PLAYER_LEDS_H
#define PLAYER_LEDS_H

enum PlayerLedState
{
    PLAYER_LED_IDLE,
    PLAYER_LED_PLAYING,
    PLAYER_LED_PAUSED,
    PLAYER_LED_SELECTING
};

// External LEDs, each through its own 330-ohm resistor to the LED anode:
// RED PA8 / P2 pin 7, GREEN PA1 / P2 pin 11, BLUE PB14 / P2 pin 13.
// All cathodes connect to GND / P2 pin 6. HIGH = on; LOW = off.
void PlayerLeds_Init();

// Single-owner, nonblocking GPIO update. An unchanged state performs no writes.
// Later the LCD/LED task can own this same call without changing the GPIO module.
void PlayerLeds_SetState(PlayerLedState state);

#endif
