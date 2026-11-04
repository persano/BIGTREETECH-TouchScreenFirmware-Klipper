#include "KlipperMacros.h"
#include "includes.h"
#include <string.h>

KLIPPER_MACROS klipperMacros = {
  .count = 0,
  .loaded = false,
  .loading = false,
};

static uint16_t kmCurPage = 0;

void klipperMacroReset(void)
{
  klipperMacros.count = 0;
  klipperMacros.loaded = false;
  klipperMacros.loading = false;
}

void klipperMacroAdd(const char * name)
{
  if (name == NULL || *name == '\0')
    return;

  if (klipperMacros.count < MAX_KLIPPER_MACROS)
  {
    strncpy_no_pad(klipperMacros.names[klipperMacros.count], name, MAX_MACRO_NAME_LEN);
    klipperMacros.names[klipperMacros.count][MAX_MACRO_NAME_LEN - 1] = '\0';
    klipperMacros.count++;
  }
}

void klipperMacroDone(void)
{
  klipperMacros.loaded = true;
  klipperMacros.loading = false;
}

static void loadMacroItem(LISTITEM * item, uint16_t index, uint8_t itemPos)
{
  if (klipperMacros.count == 0)
  {
    item->icon = klipperMacros.loading ? CHARICON_WAIT : CHARICON_INFO;
    item->itemType = LIST_LABEL;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, klipperMacros.loading ? textSelect(LABEL_LOADING) : "No Macros Found (Tap to refresh)");
    return;
  }

  if (index < klipperMacros.count)
  {
    item->icon = CHARICON_CODE;
    item->itemType = LIST_LABEL;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, klipperMacros.names[index]);
  }
  else
  {
    // Refresh item
    item->icon = CHARICON_SD_REFRESH;
    item->itemType = LIST_LABEL;
    item->titlelabel.index = LABEL_DYNAMIC;
    setDynamicLabel(itemPos, "Refresh Macros");
  }
}

void menuKlipperMacros(void)
{
  kmCurPage = 0;

  if (!klipperMacros.loaded && !klipperMacros.loading)
  {
    klipperMacros.count = 0;
    klipperMacros.loading = true;
    mustStoreCmd("M985\n");
  }

  LABEL title = {LABEL_KLIPPER_MACROS};
  uint16_t totalItems = (klipperMacros.count == 0) ? 1 : (klipperMacros.count + 1);
  listViewCreate(title, NULL, totalItems, &kmCurPage, false, NULL, loadMacroItem);

  uint8_t last_count = klipperMacros.count;
  bool last_loading = klipperMacros.loading;

  while (MENU_IS(menuKlipperMacros))
  {
    uint16_t key = listViewGetSelectedIndex();

    switch (key)
    {
      case KEY_BACK:
        CLOSE_MENU();
        break;

      default:
        if (klipperMacros.count == 0)
        {
          if (!klipperMacros.loading)
          {
            klipperMacros.loading = true;
            mustStoreCmd("M985\n");
            listViewRefreshMenu();
          }
        }
        else if (key < klipperMacros.count)
        {
          // Run selected macro
          mustStoreCmd("%s\n", klipperMacros.names[key]);
          addToast(DIALOG_TYPE_INFO, klipperMacros.names[key]);
        }
        else if (key == klipperMacros.count)
        {
          // Refresh tapped
          klipperMacros.count = 0;
          klipperMacros.loading = true;
          klipperMacros.loaded = false;
          mustStoreCmd("M985\n");
          totalItems = 1;
          listViewCreate(title, NULL, totalItems, &kmCurPage, false, NULL, loadMacroItem);
        }
        break;
    }

    if (klipperMacros.count != last_count || klipperMacros.loading != last_loading)
    {
      last_count = klipperMacros.count;
      last_loading = klipperMacros.loading;
      totalItems = (klipperMacros.count == 0) ? 1 : (klipperMacros.count + 1);
      listViewCreate(title, NULL, totalItems, &kmCurPage, false, NULL, loadMacroItem);
    }

    loopProcess();
  }
}
