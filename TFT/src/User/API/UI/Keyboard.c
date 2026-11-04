#include "Keyboard.h"
#include "includes.h"
#include <string.h>
#include <stdio.h>

void toHex(const char * in, char * out, size_t max_out)
{
  static const char hex_digits[] = "0123456789ABCDEF";
  size_t i = 0, j = 0;
  while (in[i] != '\0' && j + 2 < max_out)
  {
    uint8_t c = (uint8_t) in[i++];
    out[j++] = hex_digits[(c >> 4) & 0x0F];
    out[j++] = hex_digits[c & 0x0F];
  }
  out[j] = '\0';
}

#define KB_COLS 7
#define KB_ROWS 4
#define KB_GRID_KEYS (KB_COLS * KB_ROWS) // 28
#define KB_CTRL_KEYS 6                   // CAPS, MODE, SPACE, DEL, OK, CANCEL
#define KB_TOTAL_KEYS (KB_GRID_KEYS + KB_CTRL_KEYS)

enum {
  KEY_CTRL_CAPS = KB_GRID_KEYS,
  KEY_CTRL_MODE,
  KEY_CTRL_SPACE,
  KEY_CTRL_DEL,
  KEY_CTRL_OK,
  KEY_CTRL_CANCEL,
};

static const char * const keys_lower[KB_GRID_KEYS] = {
  "a", "b", "c", "d", "e", "f", "g",
  "h", "i", "j", "k", "l", "m", "n",
  "o", "p", "q", "r", "s", "t", "u",
  "v", "w", "x", "y", "z", ".", "-"
};

static const char * const keys_upper[KB_GRID_KEYS] = {
  "A", "B", "C", "D", "E", "F", "G",
  "H", "I", "J", "K", "L", "M", "N",
  "O", "P", "Q", "R", "S", "T", "U",
  "V", "W", "X", "Y", "Z", "_", "/"
};

static const char * const keys_sym[KB_GRID_KEYS] = {
  "1", "2", "3", "4", "5", "6", "7",
  "8", "9", "0", "!", "@", "#", "$",
  "%", "^", "&", "*", "(", ")", "+",
  "=", ":", ";", "\"", "'", "?", ","
};

static GUI_RECT kbKeyRect[KB_TOTAL_KEYS];
static GUI_RECT kbTextBoxRect;

static void initKeyRects(void)
{
  uint16_t top_h = 36;
  uint16_t bot_h = 36;
  uint16_t grid_y0 = top_h + 2;
  uint16_t grid_h = (LCD_HEIGHT - bot_h - 2) - grid_y0;
  uint16_t row_h = grid_h / KB_ROWS;
  uint16_t col_w = LCD_WIDTH / KB_COLS;

  kbTextBoxRect.x0 = 4;
  kbTextBoxRect.y0 = 2;
  kbTextBoxRect.x1 = LCD_WIDTH - 4;
  kbTextBoxRect.y1 = top_h;

  for (uint8_t r = 0; r < KB_ROWS; r++)
  {
    for (uint8_t c = 0; c < KB_COLS; c++)
    {
      uint8_t i = r * KB_COLS + c;
      kbKeyRect[i].x0 = c * col_w + 1;
      kbKeyRect[i].y0 = grid_y0 + r * row_h + 1;
      kbKeyRect[i].x1 = (c == KB_COLS - 1) ? (LCD_WIDTH - 1) : ((c + 1) * col_w - 1);
      kbKeyRect[i].y1 = grid_y0 + (r + 1) * row_h - 1;
    }
  }

  uint16_t bot_y0 = LCD_HEIGHT - bot_h;
  uint16_t ctrl_w = LCD_WIDTH / KB_CTRL_KEYS;

  for (uint8_t c = 0; c < KB_CTRL_KEYS; c++)
  {
    uint8_t i = KB_GRID_KEYS + c;
    kbKeyRect[i].x0 = c * ctrl_w + 1;
    kbKeyRect[i].y0 = bot_y0 + 1;
    kbKeyRect[i].x1 = (c == KB_CTRL_KEYS - 1) ? (LCD_WIDTH - 1) : ((c + 1) * ctrl_w - 1);
    kbKeyRect[i].y1 = LCD_HEIGHT - 1;
  }
}

static uint8_t kb_mode = 0; // 0 = lower, 1 = upper, 2 = symbols

static void drawKbButton(uint8_t index, uint8_t isPressed)
{
  if (index >= KB_TOTAL_KEYS)
    return;

  const char * label = "";
  uint16_t bg = isPressed ? MAT_LOWWHITE : 0x2124; // dark gray
  uint16_t fg = isPressed ? BLACK : WHITE;

  if (index < KB_GRID_KEYS)
  {
    if (kb_mode == 0)
      label = keys_lower[index];
    else if (kb_mode == 1)
      label = keys_upper[index];
    else
      label = keys_sym[index];
  }
  else
  {
    bg = isPressed ? MAT_LOWWHITE : 0x3186; // control button color
    switch (index)
    {
      case KEY_CTRL_CAPS:
        label = (kb_mode == 1) ? "caps" : "CAPS";
        break;
      case KEY_CTRL_MODE:
        label = (kb_mode == 2) ? "ABC" : "123";
        break;
      case KEY_CTRL_SPACE:
        label = "Space";
        break;
      case KEY_CTRL_DEL:
        label = "Del";
        bg = isPressed ? MAT_LOWWHITE : MAT_RED;
        break;
      case KEY_CTRL_OK:
        label = "OK";
        bg = isPressed ? MAT_LOWWHITE : MAT_GREEN;
        break;
      case KEY_CTRL_CANCEL:
        label = "Cancel";
        bg = isPressed ? MAT_LOWWHITE : 0x4208;
        break;
    }
  }

  GUI_SetColor(bg);
  GUI_FillPrect(&kbKeyRect[index]);
  GUI_SetColor(fg);
  GUI_SetBkColor(bg);
  GUI_DispStringInPrect(&kbKeyRect[index], (uint8_t *) label);
}

static void drawKbText(const char * title, const char * text)
{
  GUI_SetColor(0x18C3);
  GUI_FillPrect(&kbTextBoxRect);
  GUI_SetColor(WHITE);
  GUI_DrawPrect(&kbTextBoxRect);

  char disp[80];
  if (title != NULL && title[0] != '\0')
    snprintf(disp, sizeof(disp), "%s: %s_", title, text);
  else
    snprintf(disp, sizeof(disp), "%s_", text);

  GUI_SetBkColor(0x18C3);
  GUI_DispStringInPrect(&kbTextBoxRect, (uint8_t *) disp);
}

bool keyboardGetText(const char * title, char * target_buf, uint16_t max_len)
{
  if (target_buf == NULL || max_len == 0)
    return false;

  initKeyRects();

  char text[65] = {0};
  strncpy_no_pad(text, target_buf, sizeof(text));
  uint16_t len = strlen(text);

  GUI_Clear(BLACK);
  drawKbText(title, text);

  setMenu(MENU_TYPE_FULLSCREEN, NULL, KB_TOTAL_KEYS, kbKeyRect, drawKbButton, NULL);

  for (uint8_t i = 0; i < KB_TOTAL_KEYS; i++)
  {
    drawKbButton(i, false);
  }

  bool running = true;
  bool result = false;

  while (running)
  {
    uint16_t key = menuKeyGetValue();

    if (key != IDLE_TOUCH && key < KB_TOTAL_KEYS)
    {
      if (key < KB_GRID_KEYS)
      {
        const char * ch_str = "";
        if (kb_mode == 0)
          ch_str = keys_lower[key];
        else if (kb_mode == 1)
          ch_str = keys_upper[key];
        else
          ch_str = keys_sym[key];

        if (len + 1 < max_len && len + 1 < sizeof(text))
        {
          text[len++] = ch_str[0];
          text[len] = '\0';
          drawKbText(title, text);
          BUZZER_PLAY(SOUND_KEYPRESS);
        }
        else
        {
          BUZZER_PLAY(SOUND_DENY);
        }
      }
      else
      {
        switch (key)
        {
          case KEY_CTRL_CAPS:
            kb_mode = (kb_mode == 1) ? 0 : 1;
            for (uint8_t i = 0; i < KB_TOTAL_KEYS; i++)
              drawKbButton(i, false);
            BUZZER_PLAY(SOUND_KEYPRESS);
            break;

          case KEY_CTRL_MODE:
            kb_mode = (kb_mode == 2) ? 0 : 2;
            for (uint8_t i = 0; i < KB_TOTAL_KEYS; i++)
              drawKbButton(i, false);
            BUZZER_PLAY(SOUND_KEYPRESS);
            break;

          case KEY_CTRL_SPACE:
            if (len + 1 < max_len && len + 1 < sizeof(text))
            {
              text[len++] = ' ';
              text[len] = '\0';
              drawKbText(title, text);
              BUZZER_PLAY(SOUND_KEYPRESS);
            }
            break;

          case KEY_CTRL_DEL:
            if (len > 0)
            {
              text[--len] = '\0';
              drawKbText(title, text);
              BUZZER_PLAY(SOUND_KEYPRESS);
            }
            else
            {
              BUZZER_PLAY(SOUND_DENY);
            }
            break;

          case KEY_CTRL_OK:
            strncpy_no_pad(target_buf, text, max_len);
            target_buf[max_len - 1] = '\0';
            result = true;
            running = false;
            BUZZER_PLAY(SOUND_OK);
            break;

          case KEY_CTRL_CANCEL:
            result = false;
            running = false;
            BUZZER_PLAY(SOUND_CANCEL);
            break;
        }
      }
    }

    loopBackEnd();
  }

  GUI_RestoreColorDefault();
  return result;
}
