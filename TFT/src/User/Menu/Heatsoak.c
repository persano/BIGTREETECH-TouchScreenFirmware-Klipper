#include "Heatsoak.h"
#include "includes.h"
#include <stdio.h>

HEATSOAK_STATE heatsoakState = {
  .active = false,
  .duration_sec = 300,
  .remaining_sec = 300,
  .target_bed_temp = 60,
};

void heatsoakStart(uint16_t duration_sec, int16_t target_temp)
{
  heatsoakState.duration_sec = (duration_sec > 0) ? duration_sec : 300;
  heatsoakState.remaining_sec = heatsoakState.duration_sec;
  heatsoakState.target_bed_temp = target_temp;
  heatsoakState.active = true;
}

void heatsoakStop(void)
{
  heatsoakState.active = false;
  heatsoakState.remaining_sec = 0;
}

#define SOAK_BTN_COUNT 4
static GUI_RECT soakBtnRect[SOAK_BTN_COUNT];

static void initSoakButtons(void)
{
  uint16_t btn_w = LCD_WIDTH / SOAK_BTN_COUNT;
  uint16_t btn_h = 42;
  uint16_t btn_y0 = LCD_HEIGHT - btn_h - 4;

  for (uint8_t i = 0; i < SOAK_BTN_COUNT; i++)
  {
    soakBtnRect[i].x0 = i * btn_w + 2;
    soakBtnRect[i].y0 = btn_y0;
    soakBtnRect[i].x1 = (i + 1) * btn_w - 2;
    soakBtnRect[i].y1 = LCD_HEIGHT - 4;
  }
}

static void drawSoakButton(uint8_t index, uint8_t isPressed)
{
  if (index >= SOAK_BTN_COUNT)
    return;

  const char * labels[SOAK_BTN_COUNT] = {"-1 Min", "+1 Min", "Skip", "Back"};
  uint16_t bg = isPressed ? MAT_LOWWHITE : 0x2965; // steel blue
  if (index == 2) bg = isPressed ? MAT_LOWWHITE : MAT_GREEN;
  if (index == 3) bg = isPressed ? MAT_LOWWHITE : MAT_RED;

  GUI_SetColor(bg);
  GUI_FillPrect(&soakBtnRect[index]);
  GUI_SetColor(WHITE);
  GUI_SetBkColor(bg);
  GUI_DispStringInPrect(&soakBtnRect[index], (uint8_t *) labels[index]);
}

void menuHeatsoak(void)
{
  if (!heatsoakState.active)
  {
    int16_t bed_target = heatGetTargetTemp(BED);
    heatsoakStart(300, (bed_target > 0) ? bed_target : 60);
  }

  initSoakButtons();
  setMenu(MENU_TYPE_FULLSCREEN, NULL, SOAK_BTN_COUNT, soakBtnRect, drawSoakButton, NULL);

  GUI_Clear(BLACK);

  for (uint8_t i = 0; i < SOAK_BTN_COUNT; i++)
  {
    drawSoakButton(i, false);
  }

  uint32_t last_tick = OS_GetTimeMs();
  char str_buf[64];

  GUI_RECT titleRect = {0, 8, LCD_WIDTH, 32};
  GUI_RECT tempRect = {0, 42, LCD_WIDTH, 74};
  GUI_RECT timerRect = {0, 80, LCD_WIDTH, 120};
  GUI_RECT barBorder = {20, 130, LCD_WIDTH - 20, 150};

  GUI_SetColor(WHITE);
  GUI_SetBkColor(BLACK);
  GUI_DispStringInPrect(&titleRect, (uint8_t *) "Bed Heatsoak in Progress");

  while (MENU_IS(menuHeatsoak))
  {
    uint16_t key = menuKeyGetValue();

    switch (key)
    {
      case 0: // -1 Min
        if (heatsoakState.remaining_sec > 60)
          heatsoakState.remaining_sec -= 60;
        else
          heatsoakState.remaining_sec = 0;
        BUZZER_PLAY(SOUND_KEYPRESS);
        break;

      case 1: // +1 Min
        heatsoakState.remaining_sec += 60;
        heatsoakState.duration_sec += 60;
        BUZZER_PLAY(SOUND_KEYPRESS);
        break;

      case 2: // Skip
        heatsoakStop();
        BUZZER_PLAY(SOUND_OK);
        CLOSE_MENU();
        break;

      case 3: // Back / Cancel
        heatsoakStop();
        BUZZER_PLAY(SOUND_CANCEL);
        CLOSE_MENU();
        break;

      default:
        break;
    }

    if (OS_GetTimeMs() - last_tick >= 1000)
    {
      last_tick = OS_GetTimeMs();

      int16_t cur_bed = heatGetCurrentTemp(BED);
      int16_t target_bed = heatGetTargetTemp(BED);

      // Bed temperature display
      snprintf(str_buf, sizeof(str_buf), "Bed: %dC / %dC", cur_bed, (target_bed > 0 ? target_bed : heatsoakState.target_bed_temp));
      GUI_SetColor(BLACK);
      GUI_FillPrect(&tempRect);
      GUI_SetColor(MAT_YELLOW);
      GUI_SetBkColor(BLACK);
      GUI_DispStringInPrect(&tempRect, (uint8_t *) str_buf);

      // Countdown display
      uint16_t mins = heatsoakState.remaining_sec / 60;
      uint16_t secs = heatsoakState.remaining_sec % 60;
      snprintf(str_buf, sizeof(str_buf), "%02d:%02d Remaining", mins, secs);

      GUI_SetColor(BLACK);
      GUI_FillPrect(&timerRect);
      GUI_SetColor(WHITE);
      GUI_SetBkColor(BLACK);
      GUI_DispStringInPrect(&timerRect, (uint8_t *) str_buf);

      // Progress bar
      GUI_SetColor(GRAY);
      GUI_DrawPrect(&barBorder);

      float progress = 1.0f;
      if (heatsoakState.duration_sec > 0)
        progress = 1.0f - ((float) heatsoakState.remaining_sec / (float) heatsoakState.duration_sec);
      if (progress < 0.0f) progress = 0.0f;
      if (progress > 1.0f) progress = 1.0f;

      uint16_t fill_x = barBorder.x0 + 2 + (uint16_t) ((barBorder.x1 - barBorder.x0 - 4) * progress);
      GUI_RECT barFill = {barBorder.x0 + 2, barBorder.y0 + 2, fill_x, barBorder.y1 - 2};
      GUI_RECT barEmpty = {fill_x, barBorder.y0 + 2, barBorder.x1 - 2, barBorder.y1 - 2};

      GUI_SetColor(MAT_GREEN);
      GUI_FillPrect(&barFill);
      GUI_SetColor(BLACK);
      GUI_FillPrect(&barEmpty);

      // Decrement countdown once bed is within 2C of target
      if (cur_bed >= (heatsoakState.target_bed_temp - 2))
      {
        if (heatsoakState.remaining_sec > 0)
        {
          heatsoakState.remaining_sec--;
        }
        else
        {
          heatsoakStop();
          BUZZER_PLAY(SOUND_OK);
          CLOSE_MENU();
          break;
        }
      }
    }

    loopProcess();
  }

  GUI_RestoreColorDefault();
}
