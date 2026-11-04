#include "main.h"
#include "includes.h"

int main(void)
{
  SysTick->CTRL = 0;           // disable bootloader's SysTick
  SysTick->LOAD = 0;
  SysTick->VAL  = 0;

  for (uint8_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFF;  // disable all interrupts leftover from bootloader
    NVIC->ICPR[i] = 0xFFFFFFFF;  // clear all pending interrupts
  }

  SCB->VTOR = VECT_TAB_FLASH;  // set before __enable_irq so interrupts use our vectors
  __DSB();
  __ISB();

  #ifdef GD32F3XX
    __enable_irq();
  #endif

  SystemClockInit();  // it depends on "variants.h" included in "includes.h"

  HW_Init();

  #if defined(SERIAL_DEBUG_PORT) && defined(SERIAL_DEBUG_ENABLED)
    dbg_print("Main Startup: Generic debug output is enabled.\n");
  #endif

  for (; ;)
  {
    (*infoMenu.menu[infoMenu.cur])();
  }
}
