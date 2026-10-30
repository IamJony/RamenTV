#include "themes.h"
#include <FL/Fl.H>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <string>

Fl_Color C_BG, C_BAR, C_SIDE, C_PANEL, C_BTN, C_BTN_HI, C_SEL,
         C_TEXT, C_DIM, C_ACCENT, C_RED, C_GREEN, C_LINE;
int g_theme_index = 8;
std::function<void()> g_refresh_cb;

const Theme themes[] = {
    {"Alienware", fl_rgb_color(10,14,20), fl_rgb_color(15,22,32), fl_rgb_color(26,36,48), fl_rgb_color(26,36,48), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(0,229,255), fl_rgb_color(230,241,255), fl_rgb_color(122,143,166), fl_rgb_color(0,229,255), fl_rgb_color(255,45,85), fl_rgb_color(0,255,159), fl_rgb_color(10,14,20)},
    {"Neon Rosa", fl_rgb_color(13,5,18), fl_rgb_color(24,10,32), fl_rgb_color(37,18,50), fl_rgb_color(37,18,50), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(255,43,214), fl_rgb_color(255,228,249), fl_rgb_color(156,111,160), fl_rgb_color(255,43,214), fl_rgb_color(255,23,68), fl_rgb_color(255,138,232), fl_rgb_color(13,5,18)},
    {"Neon Verde", fl_rgb_color(2,10,3), fl_rgb_color(6,19,8), fl_rgb_color(13,31,17), fl_rgb_color(13,31,17), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(0,255,65), fl_rgb_color(212,255,214), fl_rgb_color(90,143,94), fl_rgb_color(0,255,65), fl_rgb_color(255,23,68), fl_rgb_color(124,255,138), fl_rgb_color(2,10,3)},
    {"Neon Morado", fl_rgb_color(10,5,24), fl_rgb_color(18,10,36), fl_rgb_color(29,18,56), fl_rgb_color(29,18,56), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(168,85,247), fl_rgb_color(236,228,255), fl_rgb_color(125,111,168), fl_rgb_color(168,85,247), fl_rgb_color(255,23,68), fl_rgb_color(192,132,252), fl_rgb_color(10,5,24)},
    {"Neon Naranja", fl_rgb_color(16,7,3), fl_rgb_color(28,13,5), fl_rgb_color(42,22,10), fl_rgb_color(42,22,10), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(255,140,26), fl_rgb_color(255,234,212), fl_rgb_color(168,119,96), fl_rgb_color(255,140,26), fl_rgb_color(255,23,68), fl_rgb_color(255,190,92), fl_rgb_color(16,7,3)},
    {"Nord", fl_rgb_color(46,52,64), fl_rgb_color(59,66,82), fl_rgb_color(67,76,94), fl_rgb_color(67,76,94), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(136,192,208), fl_rgb_color(236,239,244), fl_rgb_color(123,136,161), fl_rgb_color(136,192,208), fl_rgb_color(191,97,106), fl_rgb_color(163,190,140), fl_rgb_color(46,52,64)},
    {"Dracula", fl_rgb_color(40,42,54), fl_rgb_color(33,34,44), fl_rgb_color(52,55,70), fl_rgb_color(52,55,70), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(189,147,249), fl_rgb_color(248,248,242), fl_rgb_color(98,114,164), fl_rgb_color(189,147,249), fl_rgb_color(255,85,85), fl_rgb_color(80,250,123), fl_rgb_color(40,42,54)},
    {"Tokyo Night", fl_rgb_color(26,27,38), fl_rgb_color(22,22,30), fl_rgb_color(36,40,59), fl_rgb_color(36,40,59), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(122,162,247), fl_rgb_color(192,202,245), fl_rgb_color(86,95,137), fl_rgb_color(122,162,247), fl_rgb_color(247,118,142), fl_rgb_color(158,206,106), fl_rgb_color(26,27,38)},
    {"Gruvbox", fl_rgb_color(29,32,33), fl_rgb_color(40,40,40), fl_rgb_color(60,56,54), fl_rgb_color(60,56,54), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(250,189,47), fl_rgb_color(235,219,178), fl_rgb_color(146,131,116), fl_rgb_color(250,189,47), fl_rgb_color(251,73,52), fl_rgb_color(184,187,38), fl_rgb_color(29,32,33)},
    {"Catppuccin Mocha", fl_rgb_color(30,30,46), fl_rgb_color(24,24,37), fl_rgb_color(49,50,68), fl_rgb_color(49,50,68), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(203,166,247), fl_rgb_color(205,214,244), fl_rgb_color(108,112,134), fl_rgb_color(203,166,247), fl_rgb_color(243,139,168), fl_rgb_color(166,227,161), fl_rgb_color(30,30,46)},
    {"One Dark", fl_rgb_color(40,44,52), fl_rgb_color(33,37,43), fl_rgb_color(58,63,75), fl_rgb_color(58,63,75), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(97,175,239), fl_rgb_color(171,178,191), fl_rgb_color(92,99,112), fl_rgb_color(97,175,239), fl_rgb_color(224,108,117), fl_rgb_color(152,195,121), fl_rgb_color(40,44,52)},
    {"Rose Pine", fl_rgb_color(25,23,36), fl_rgb_color(31,29,46), fl_rgb_color(38,35,58), fl_rgb_color(38,35,58), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(235,188,186), fl_rgb_color(224,222,244), fl_rgb_color(110,106,134), fl_rgb_color(235,188,186), fl_rgb_color(235,111,146), fl_rgb_color(156,207,216), fl_rgb_color(25,23,36)},
    {"Kanagawa", fl_rgb_color(31,31,40), fl_rgb_color(22,22,29), fl_rgb_color(42,42,55), fl_rgb_color(42,42,55), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(126,156,216), fl_rgb_color(220,215,186), fl_rgb_color(114,113,105), fl_rgb_color(126,156,216), fl_rgb_color(195,64,67), fl_rgb_color(118,148,106), fl_rgb_color(31,31,40)},
    {"GitHub Dark", fl_rgb_color(13,17,23), fl_rgb_color(22,27,34), fl_rgb_color(33,38,45), fl_rgb_color(33,38,45), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(88,166,255), fl_rgb_color(230,237,243), fl_rgb_color(125,133,144), fl_rgb_color(88,166,255), fl_rgb_color(248,81,73), fl_rgb_color(63,185,80), fl_rgb_color(13,17,23)},
    {"Cyberpunk", fl_rgb_color(13,2,8), fl_rgb_color(21,5,16), fl_rgb_color(31,8,24), fl_rgb_color(31,8,24), fl_rgb_color(50,50,50), fl_rgb_color(70,70,70), fl_rgb_color(255,0,60), fl_rgb_color(240,230,239), fl_rgb_color(122,90,120), fl_rgb_color(255,0,60), fl_rgb_color(255,0,60), fl_rgb_color(0,255,159), fl_rgb_color(13,2,8)}
};
const int theme_count = sizeof(themes) / sizeof(Theme);

void apply_theme(int index) {
    if (index < 0 || index >= theme_count) return;
    const Theme& t = themes[index];
    C_BG=t.bg; C_BAR=t.bar; C_SIDE=t.side; C_PANEL=t.panel; C_BTN=t.btn; C_BTN_HI=t.btn_hi;
    C_SEL=t.sel; C_TEXT=t.text; C_DIM=t.dim; C_ACCENT=t.accent; C_RED=t.red; C_GREEN=t.green; C_LINE=t.line;
    Fl::set_color(FL_BACKGROUND_COLOR,  t.panel ? t.panel : t.bg);
    Fl::set_color(FL_BACKGROUND2_COLOR, t.bg);
    Fl::set_color(FL_FOREGROUND_COLOR,  t.text);
    Fl::set_color(FL_SELECTION_COLOR,   t.sel);
    Fl_Color _bg = t.panel ? t.panel : t.bg;
    Fl::set_color(FL_INACTIVE_COLOR, fl_lighter(_bg));
    Fl::set_color(FL_DARK3, fl_darker(_bg));
    Fl::set_color(FL_DARK2, fl_darker(fl_darker(_bg)));
    Fl::set_color(FL_DARK1, fl_darker(fl_darker(fl_darker(_bg))));
    if (g_refresh_cb) g_refresh_cb();
}

static std::string theme_path() {
    const char* h = getenv("HOME");
    std::string dir = std::string(h ? h : "/tmp") + "/.config/ramentv-fltk";
    mkdir(dir.c_str(), 0700);
    return dir + "/theme";
}

int load_saved_theme() {
    FILE* f = fopen(theme_path().c_str(), "r");
    if (!f) return 8;
    int idx = 8;
    if (fscanf(f, "%d", &idx) != 1) idx = 8;
    fclose(f);
    if (idx < 0 || idx >= theme_count) idx = 8;
    return idx;
}

void save_theme(int index) {
    if (index < 0 || index >= theme_count) return;
    FILE* f = fopen(theme_path().c_str(), "w");
    if (!f) return;
    fprintf(f, "%d\n", index);
    fclose(f);
}
