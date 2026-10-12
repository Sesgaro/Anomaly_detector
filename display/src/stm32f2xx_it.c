#include "board.h"
#include "hw.h"
#include "link.h"
#include "knob.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
    hw_tick_1ms();
    knob_tick_1ms();
}

void USART2_IRQHandler(void)
{
    link_irq();
}

void HardFault_Handler(void)
{
    for (;;) {}
}
