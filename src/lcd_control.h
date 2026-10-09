#ifndef RT_SPARK_LCD_CONTROL_H
#define RT_SPARK_LCD_CONTROL_H

#if defined(RTOS_STAGE5_PLAYER) && RTOS_STAGE5_PLAYER
void LcdControl_Init();
void LcdControl_BindOwner();
void LcdControl_Lock();
void LcdControl_Unlock();
#endif

#endif
