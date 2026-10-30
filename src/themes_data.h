#ifndef THEMES_DATA_H
#define THEMES_DATA_H
#include <FL/Fl.H>
struct Theme {
    const char* name;
    Fl_Color bg, bar, side, panel, btn, btn_hi, sel, text, dim, accent, red, green, line;
};
extern const Theme themes[];
extern const int theme_count;
#endif
