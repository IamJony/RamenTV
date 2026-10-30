#ifndef THEMES_H
#define THEMES_H
#include "themes_data.h"
#include <functional>
extern Fl_Color C_BG, C_BAR, C_SIDE, C_PANEL, C_BTN, C_BTN_HI, C_SEL,
                C_TEXT, C_DIM, C_ACCENT, C_RED, C_GREEN, C_LINE;
extern int g_theme_index;
extern std::function<void()> g_refresh_cb;
void apply_theme(int index);
int  load_saved_theme();
void save_theme(int index);
#endif
