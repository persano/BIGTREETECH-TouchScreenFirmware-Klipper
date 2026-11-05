#include "main.h"
#include "includes.h"

int main(void)
{
  SCB->VTOR = VECT_TAB_FLASH;
  __DSB();
  __ISB();

  __enable_irq();

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
