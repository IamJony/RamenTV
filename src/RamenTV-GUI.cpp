#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include "themes.h"
#include "library.h"
#include "cJSON.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <array>
#include <regex>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <unordered_map>

static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static std::string run_cmd_timeout(const std::string& cmd, int timeout_sec = 120) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return ""; }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    std::string result;
    char buf[4096];
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_sec) {
            kill(pid, SIGKILL);
            break;
        }
        struct pollfd pfd{pipefd[0], POLLIN, 0};
        int pr = poll(&pfd, 1, 500);
        if (pr < 0 && errno != EINTR) break;
        if (pr == 0) continue;
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n > 0) result.append(buf, n);
        else if (n == 0) break;
        else if (errno != EINTR) break;
    }
    close(pipefd[0]);
    int status = 0;
    if (waitpid(pid, &status, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return result;
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r')) --b;
    return s.substr(a, b-a);
}

static std::string json_get_str(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
    std::smatch m;
    if (std::regex_search(json, m, re)) {
        std::string v = m[1].str(), out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size()) {
                char n = v[i+1];
                if (n=='/') { out+='/'; ++i; continue; }
                if (n=='"') { out+='"'; ++i; continue; }
                if (n=='n') { out+='\n'; ++i; continue; }
                if (n=='t') { out+='\t'; ++i; continue; }
            }
            out += v[i];
        }
        return out;
    }
    return "";
}

static std::string json_get_int(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch m;
    if (std::regex_search(json, m, re)) return m[1].str();
    return "";
}

static std::vector<AnimeItem> parse_search_json(const std::string& json) {
    std::vector<AnimeItem> out;
    if (json.empty()) return out;
    std::regex title_re("\"title\"\\s*:\\s*\"");
    auto end = std::sregex_iterator();
    size_t search_from = 0;
    while (search_from < json.size()) {
        auto it = std::sregex_iterator(json.begin() + search_from, json.end(), title_re);
        if (it == end) break;
        size_t title_pos = search_from + (size_t)it->position();
        size_t brace_open = std::string::npos;
        int depth = 0;
        for (size_t i = title_pos; i > 0; --i) {
            char c = json[i-1];
            if (c == '}') ++depth;
            else if (c == '{') {
                if (depth == 0) { brace_open = i - 1; break; }
                --depth;
            }
        }
        if (brace_open == std::string::npos) { search_from = title_pos + 1; continue; }
        size_t brace_close = std::string::npos;
        depth = 0;
        for (size_t i = brace_open; i < json.size(); ++i) {
            char c = json[i];
            if (c == '{') ++depth;
            else if (c == '}') {
                if (--depth == 0) { brace_close = i; break; }
            }
        }
        if (brace_close == std::string::npos) { search_from = title_pos + 1; continue; }
        std::string chunk = json.substr(brace_open, brace_close - brace_open + 1);
        AnimeItem a;
        a.title = json_get_str(chunk, "title");
        if (a.title.empty()) { search_from = brace_close + 1; continue; }
        a.id   = json_get_str(chunk, "id");
        if (a.id.empty()) a.id = json_get_int(chunk, "id");
        a.slug = json_get_str(chunk, "slug");
        a.img = json_get_str(chunk, "thumbnail");
        if (a.img.empty()) a.img = json_get_str(chunk, "img");
        if (a.img.empty()) a.img = json_get_str(chunk, "cover");
        if (a.img.empty()) a.img = json_get_str(chunk, "image");
        a.thumbnail = a.img;
        a.year = json_get_str(chunk, "year");
        if (a.year.empty()) a.year = json_get_str(chunk, "date");
        a.url  = json_get_str(chunk, "url");
        a.imdb = json_get_str(chunk, "type");
        if (a.imdb.empty()) a.imdb = json_get_str(chunk, "genre");
        if (a.id.empty()) {
            std::hash<std::string> h;
            a.id = std::to_string(h(a.title));
        }
        out.push_back(std::move(a));
        search_from = brace_close + 1;
    }
    return out;
}

struct ServerItem {
    std::string url;
    std::string name;
};

static std::vector<ServerItem> parse_servers_json(const std::string& json) {
    std::vector<ServerItem> out;
    if (json.empty()) return out;
    
    cJSON *root = cJSON_Parse(json.c_str());
    if (!root) {
        fprintf(stderr, "[RamenTV] Error al parsear servers.json con cJSON\n");
        return out;
    }
    
    cJSON *servers_array = cJSON_GetObjectItemCaseSensitive(root, "servers");
    if (cJSON_IsArray(servers_array)) {
        cJSON *server_item = nullptr;
        cJSON_ArrayForEach(server_item, servers_array) {
            if (cJSON_IsString(server_item) && server_item->valuestring != nullptr) {
                ServerItem s;
                s.url = server_item->valuestring;
                
                std::regex host_re("^https?://([^/:]+)");
                std::smatch hm;
                if (std::regex_search(s.url, hm, host_re)) {
                    std::string host = hm[1].str();
                    if (host.rfind("www.", 0) == 0) host = host.substr(4);
                    auto dot = host.find('.');
                    s.name = (dot != std::string::npos) ? host.substr(0, dot) : host;
                    if (!s.name.empty()) s.name[0] = std::toupper(s.name[0]);
                } else {
                    s.name = "Servidor " + std::to_string(out.size() + 1);
                }
                out.push_back(s);
            }
        }
    } else {
        fprintf(stderr, "[RamenTV] 'servers' no es un array en servers.json\n");
    }
    
    cJSON_Delete(root);
    return out;
}

class BgImage : public Fl_Box {
public:
    Fl_Shared_Image* img = nullptr;
    BgImage(int x, int y, int w, int h) : Fl_Box(x, y, w, h) { box(FL_NO_BOX); }
    void set_image(Fl_Shared_Image* i) { img = i; redraw(); }
    void draw() override {
        if (!img || img->w() <= 0) return;
        int pw = img->w(), ph = img->h();
        double ar_img = (double)pw / ph;
        double ar_box = (double)w() / h();
        int dw, dh;
        if (ar_img > ar_box) { dh = h(); dw = (int)(dh * ar_img); }
        else                 { dw = w(); dh = (int)(dw / ar_img); }
        if (dw <= 0 || dh <= 0) return;
        Fl_Image* scaled = img->copy(dw, dh);
        if (!scaled) return;
        int dx = x() + (w() - dw) / 2;
        int dy = y() + (h() - dh) / 2;
        fl_push_clip(x(), y(), w(), h());
        scaled->draw(dx, dy);
        fl_pop_clip();
        delete scaled;
    }
};

class ModernButton : public Fl_Button {
public:
    Fl_Color bg = C_BTN, hi = C_BTN_HI, fg = C_TEXT;
    bool hover = false, selected = false;
    int radius = 6;
    ModernButton(int x, int y, int w, int h, const char* l = nullptr)
        : Fl_Button(x, y, w, h, l) { box(FL_NO_BOX); clear_visible_focus(); }
    void draw() override {
        Fl_Color c = selected ? C_SEL : (hover ? hi : bg);
        int r = radius;
        fl_color(c);
        fl_rectf(x(), y() + r, w(), h() - 2*r);
        fl_rectf(x() + r, y(), w() - 2*r, h());
        fl_pie(x(), y(), 2*r, 2*r, 90, 180);
        fl_pie(x() + w() - 2*r, y(), 2*r, 2*r, 0, 90);
        fl_pie(x(), y() + h() - 2*r, 2*r, 2*r, 180, 270);
        fl_pie(x() + w() - 2*r, y() + h() - 2*r, 2*r, 2*r, 270, 360);
        if (label()) {
            fl_font(FL_HELVETICA_BOLD, 12);
            fl_color(active_r() ? fg : C_DIM);
            fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
        }
    }
    int handle(int e) override {
        if (e == FL_ENTER || e == FL_LEAVE) { hover = (e == FL_ENTER); redraw(); }
        return Fl_Button::handle(e);
    }
};

class Label : public Fl_Widget {
public:
    std::string text;
    int size = 13;
    Fl_Font font = FL_HELVETICA;
    int fg_role = 0;
    Fl_Align al = FL_ALIGN_LEFT;
    Label(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
    void set(const std::string& t) { text = t; redraw(); if (parent()) parent()->redraw(); }
    void draw() override {
        fl_font(font, size);
        Fl_Color c = (fg_role == 0) ? C_TEXT : (fg_role == 1) ? C_DIM :
                     (fg_role == 2) ? C_ACCENT : (fg_role == 3) ? C_GREEN : C_RED;
        fl_color(c);
        fl_draw(text.c_str(), x(), y(), w(), h(), al | FL_ALIGN_INSIDE);
    }
};

class AnimeCell : public Fl_Widget {
public:
    int tag = 0;   // 0 = AnimeCell
    int index;
    AnimeItem* anime;
    bool selected = false;
    AnimeCell(int x, int y, int w, int h, AnimeItem* a, int idx)
        : Fl_Widget(x, y, w, h), index(idx), anime(a) { box(FL_NO_BOX); }
    void set_selected(bool s) { if (selected == s) return; selected = s; redraw(); }
    int handle(int event) override {
        switch (event) {
            case FL_PUSH: return 1;
            case FL_RELEASE: do_callback(); return 1;
            default: return Fl_Widget::handle(event);
        }
    }
    void draw() override {
        fl_push_clip(x(), y(), w(), h());
        int r = 6;
        fl_color(selected ? C_SEL : C_PANEL);
        fl_rectf(x(), y() + r, w(), h() - 2*r);
        fl_rectf(x() + r, y(), w() - 2*r, h());
        fl_pie(x(), y(), 2*r, 2*r, 90, 180);
        fl_pie(x() + w() - 2*r, y(), 2*r, 2*r, 0, 90);
        fl_pie(x(), y() + h() - 2*r, 2*r, 2*r, 180, 270);
        fl_pie(x() + w() - 2*r, y() + h() - 2*r, 2*r, 2*r, 270, 360);
        int text_block_h = 62;
        int img_w = w();
        int img_h = h() - text_block_h;
        if (img_h < 1) img_h = 1;
        std::string img_path = "/tmp/TEMP_A-CLI/poster_" + anime->id + ".img";
        Fl_Shared_Image* img = Fl_Shared_Image::get(img_path.c_str());
        if (img && img->w() > 0) {
            Fl_Image* scaled = img->copy(img_w, img_h);
            if (scaled) { scaled->draw(x(), y()); delete scaled; }
            else img->draw(x(), y());
        } else {
            fl_draw_box(FL_BORDER_BOX, x(), y(), img_w, img_h, C_DIM);
            fl_color(C_TEXT); fl_font(FL_HELVETICA, 10);
            fl_draw("Sin imagen", x(), y(), img_w, img_h, FL_ALIGN_CENTER);
        }
        int pad_top = 8, pad_x = 5;
        int text_y = y() + img_h + pad_top;
        int text_w = w() - pad_x * 2;
        std::string t = anime->title;
        fl_color(selected ? FL_WHITE : C_TEXT);
        fl_font(FL_HELVETICA_BOLD, 10);
        fl_push_clip(x() + pad_x, text_y, text_w, 28);
        fl_draw(t.c_str(), x() + pad_x, text_y, text_w, 28,
                FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_CLIP);
        fl_pop_clip();
        int pad_mid = 6;
        fl_color(selected ? FL_WHITE : C_DIM);
        fl_font(FL_HELVETICA, 9);
        std::string meta = anime->year;
        if (!anime->imdb.empty()) meta += "  " + anime->imdb;
        fl_draw(meta.c_str(), x() + pad_x, text_y + 28 + pad_mid, text_w, 14,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        if (selected) {
            fl_color(C_ACCENT);
            fl_rect(x(), y(), w(), h());
        }
        fl_pop_clip();
    }
};


class ChapterRow : public Fl_Widget {
public:
    int tag = 1;   // 1 = ChapterRow
    int index;
    AnimeItem* anime;
    bool selected = false;
    ChapterRow(int x, int y, int w, int h, AnimeItem* a, int idx)
        : Fl_Widget(x, y, w, h), index(idx), anime(a) { box(FL_NO_BOX); }
    void set_selected(bool s) { if (selected == s) return; selected = s; redraw(); }
    int handle(int event) override {
        switch (event) {
            case FL_PUSH: return 1;
            case FL_RELEASE: do_callback(); return 1;
            default: return Fl_Widget::handle(event);
        }
    }
    void draw() override {
        fl_push_clip(x(), y(), w(), h());
        int r = 6;
        fl_color(selected ? C_SEL : C_PANEL);
        fl_rectf(x(), y() + r, w(), h() - 2*r);
        fl_rectf(x() + r, y(), w() - 2*r, h());
        fl_pie(x(), y(), 2*r, 2*r, 90, 180);
        fl_pie(x() + w() - 2*r, y(), 2*r, 2*r, 0, 90);
        fl_pie(x(), y() + h() - 2*r, 2*r, 2*r, 180, 270);
        fl_pie(x() + w() - 2*r, y() + h() - 2*r, 2*r, 2*r, 270, 360);
        fl_color(selected ? FL_WHITE : C_ACCENT);
        fl_font(FL_HELVETICA_BOLD, 18);
        std::string num = std::to_string(index + 1);
        fl_draw(num.c_str(), x() + 12, y(), 60, h(), FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        fl_color(selected ? FL_WHITE : C_TEXT);
        fl_font(FL_HELVETICA, 13);
        fl_push_clip(x() + 70, y(), w() - 80, h());
        fl_draw(anime->title.c_str(), x() + 70, y(), w() - 80, h(),
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        fl_pop_clip();
        if (selected) {
            fl_color(C_ACCENT);
            fl_rect(x(), y(), w(), h());
        }
        fl_pop_clip();
    }
};

class MainWindow : public Fl_Window {
public:
    MainWindow(int w, int h, const char* title) : Fl_Window(w, h, title) {}
    void resize(int X, int Y, int W, int H) override {
        Fl_Window::resize(X, Y, W, H);
        if (on_resize_cb) on_resize_cb(W, H);
    }
    int handle(int e) override {
        if (e == FL_SHOW) {
            Fl::add_timeout(0.1, [](void* data) {
                auto* win = (MainWindow*)data;
                if (!win || !win->visible()) return;
                if (win->on_resize_cb) win->on_resize_cb(win->w(), win->h());
                win->redraw();
                Fl::flush();
            }, this);
        }
        return Fl_Window::handle(e);
    }
    std::function<void(int,int)> on_resize_cb;
};

static std::string find_base_dir(const std::string& relative_marker) {
    std::vector<std::string> candidates;
    if (const char* env = std::getenv("RAMENTV_HOME")) {
        if (env[0]) candidates.emplace_back(env);
    }
    auto add_with_parents = [&](std::string dir) {
        for (int i = 0; i < 6 && !dir.empty() && dir != "/"; ++i) {
            candidates.push_back(dir);
            auto sl = dir.find_last_of('/');
            if (sl == std::string::npos || sl == 0) break;
            dir = dir.substr(0, sl);
        }
    };
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        std::string exe(buf);
        auto pos = exe.find_last_of('/');
        if (pos != std::string::npos) add_with_parents(exe.substr(0, pos));
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) add_with_parents(cwd);
    candidates.push_back("/usr/local/share/ramentv");
    candidates.push_back("/usr/share/ramentv");
    candidates.push_back("/opt/ramentv");
    candidates.push_back(".");
    for (auto& c : candidates) {
        struct stat st;
        if (stat((c + "/" + relative_marker).c_str(), &st) == 0) return c;
    }
    return candidates.empty() ? "." : candidates.front();
}

struct PlayData {
    class RamenTVGUI* gui;
    std::string m3u8;
    std::string title;
    std::string chap;
};

class RamenTVGUI {
public:
    MainWindow* window;
    Fl_Input* search_input;
    ModernButton* search_btn;
    ModernButton* back_btn;
    ModernButton* fav_side_btn;
    Fl_Choice* theme_choice;
    Fl_Scroll* scroll;
    Fl_Box* scroll_spacer;
    BgImage* bg_image = nullptr;
    Fl_Shared_Image* home_shared_img = nullptr;
    std::vector<Fl_Widget*> cells;
    // 0 = AnimeCell, 1 = ChapterRow. Evita dynamic_cast con -fno-rtti.
    std::unordered_map<Fl_Widget*, int> cell_kind;

    ModernButton* play_btn;
    ModernButton* fav_btn;
    ModernButton* clean_btn;
    Label* fav_count_label = nullptr;
    Label* status_label;
    std::vector<AnimeItem> animes;
    std::vector<ServerItem> servers;
    int selected_index = -1;
    int selected_chapter = -1;
    std::string current_anime_id;
    std::string current_anime_slug;
    std::string current_anime_title;
    std::string base_dir;
    std::string cli_dir;
    std::string asset_dir;
    enum View { V_HOME, V_SEARCH, V_FAVS, V_CHAPTERS, V_SERVERS };
    View current_view = V_HOME;
    View prev_view = V_HOME;
    std::vector<AnimeItem> prev_animes;
    std::string prev_query;
    bool busy = false;
    bool searching = false;
    std::string busy_msg;
    std::mutex result_mutex;
    bool result_ready = false;
    struct SearchResult { std::string json; std::string log; std::string query; bool ok = false; } pending_result;
    struct ChaptersResult { int total = 0; std::string log; bool ok = false; } pending_chapters;
    bool chapters_ready = false;
    struct ServersResult { std::string json; std::string log; bool ok = false; } pending_servers;
    bool servers_ready = false;
    static constexpr int CELL_W = 120;
    static constexpr int CELL_H = 195;
    static constexpr int GAP = 10;
    static constexpr int PADDING = 10;
    static constexpr int SIDE_W = 110;

    RamenTVGUI(int w, int h, const char* title) {
        resolve_paths();
        fl_register_images();
        window = new MainWindow(w, h, title);
        window->on_resize_cb = [this](int W, int H) { layout(W, H); };
        Fl_Box* header = new Fl_Box(SIDE_W + 20, 10, w - SIDE_W - 200, 30, "RamenTV");
        header->labelfont(FL_BOLD); header->labelsize(22);
        header->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE); header->labelcolor(C_TEXT);
        theme_choice = new Fl_Choice(w - 130, 12, 120, 26, "Tema:");
        for (int i = 0; i < theme_count; ++i) theme_choice->add(themes[i].name);
        theme_choice->value(g_theme_index); theme_choice->callback(cb_theme, this);
        fav_side_btn = new ModernButton(10, 60, SIDE_W - 20, 36, "Favoritos");
        fav_side_btn->labelsize(11); fav_side_btn->callback(cb_show_favs, this);
        fav_count_label = new Label(10, 100, SIDE_W - 20, 16);
        fav_count_label->size = 10; fav_count_label->fg_role = 1; fav_count_label->al = FL_ALIGN_CENTER;
        fav_count_label->set("0 animes");
        back_btn = new ModernButton(w - 290, 12, 90, 26, "Volver");
        back_btn->callback(cb_back, this); back_btn->deactivate();
        search_input = new Fl_Input(SIDE_W + 70, 55, w - SIDE_W - 220, 30, "Buscar:");
        search_input->when(FL_WHEN_ENTER_KEY); search_input->callback(cb_search, this);
        search_btn = new ModernButton(w - 130, 55, 100, 30, "Buscar");
        search_btn->callback(cb_search, this);
        scroll = new Fl_Scroll(SIDE_W + 10, 100, w - SIDE_W - 20, h - 150);
        scroll->box(FL_FLAT_BOX);
        scroll_spacer = new Fl_Box(scroll->x(), scroll->y(), scroll->w() - 2, 10);
        scroll->end();
        {
            std::string img_path = asset_dir + "/home.jpeg";
            Fl_Shared_Image* home_img = Fl_Shared_Image::get(img_path.c_str());
            if (!home_img || home_img->w() <= 0) home_img = Fl_Shared_Image::get((base_dir + "/asset/home.jpeg").c_str());
            if (home_img && home_img->w() > 0) home_shared_img = home_img;
        }
        int bar_y = h - 34, bar_h = 28;
        status_label = new Label(SIDE_W + 10, bar_y, w - SIDE_W - 380, bar_h);
        status_label->font = FL_HELVETICA; status_label->size = 12; status_label->fg_role = 1;
        status_label->al = FL_ALIGN_LEFT | FL_ALIGN_INSIDE;
        status_label->set("Listo. Busca un anime o explora favoritos.");
        clean_btn = new ModernButton(w - 120, bar_y, 100, bar_h, "Limpiar");
        clean_btn->callback(cb_clean, this);
        fav_btn = new ModernButton(w - 120 - 10 - 170, bar_y, 170, bar_h, "Favoritos");
        fav_btn->callback(cb_fav, this); fav_btn->deactivate();
        play_btn = new ModernButton(w - 120 - 10 - 170 - 10 - 130, bar_y, 130, bar_h, "Reproducir");
        play_btn->callback(cb_play, this); play_btn->deactivate();
        window->end(); window->resizable(window);
        g_refresh_cb = [this]() { this->apply_colors(); };
        apply_colors(); update_fav_count(); check_environment();
        Fl::add_timeout(0.1, poll_results_cb, this);
    }
    ~RamenTVGUI() { if (g_refresh_cb) g_refresh_cb = nullptr; }
    void show() { window->show(); }

    void resolve_paths() {
        base_dir = find_base_dir("A-CLI/modules/search.sh");
        struct stat st;
        if (stat((base_dir + "/A-CLI/modules/search.sh").c_str(), &st) == 0) {
            cli_dir = base_dir + "/A-CLI/modules";
        } else {
            std::string alt = find_base_dir("search.sh");
            if (stat((alt + "/search.sh").c_str(), &st) == 0) {
                cli_dir = alt + "/modules";
                auto sl = alt.find_last_of('/');
                base_dir = (sl != std::string::npos) ? alt.substr(0, sl) : ".";
            } else {
                cli_dir = base_dir + "/A-CLI/modules";
            }
        }
        asset_dir = base_dir + "/asset";
        if (stat((asset_dir + "/home.jpeg").c_str(), &st) != 0) {
            std::string test = base_dir;
            for (int i = 0; i < 4; ++i) {
                if (stat((test + "/asset/home.jpeg").c_str(), &st) == 0) { asset_dir = test + "/asset"; break; }
                auto sl = test.find_last_of('/');
                if (sl == std::string::npos) break;
                test = test.substr(0, sl);
            }
        }
        fprintf(stderr, "[RamenTV] base_dir=%s\n", base_dir.c_str());
        fprintf(stderr, "[RamenTV] cli_dir=%s\n", cli_dir.c_str());
        fprintf(stderr, "[RamenTV] asset_dir=%s\n", asset_dir.c_str());
    }

    void check_environment() {
        std::string msg;
        struct stat st;
        if (stat(cli_dir.c_str(), &st) != 0) msg = "AVISO: no existe " + cli_dir;
        else {
            if (stat((cli_dir + "/search.sh").c_str(), &st) != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta search.sh");
            if (stat((cli_dir + "/chapters.sh").c_str(), &st) != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta chapters.sh");
            if (stat((cli_dir + "/servers.sh").c_str(), &st) != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta servers.sh");
            if (stat((cli_dir + "/m3u8.sh").c_str(), &st) != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta m3u8.sh");
        }
        if (std::system("command -v jq >/dev/null 2>&1") != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta jq");
        if (std::system("command -v curl >/dev/null 2>&1") != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta curl");
        if (std::system("command -v yt-dlp >/dev/null 2>&1") != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta yt-dlp");
        if (std::system("command -v mplayer >/dev/null 2>&1") != 0) msg += (msg.empty() ? "" : " | ") + std::string("falta mplayer");
        if (!msg.empty()) { status_label->fg_role = 4; status_label->set(msg); fprintf(stderr, "[RamenTV] %s\n", msg.c_str()); }
    }

    void set_status(const std::string& msg) { busy = false; busy_msg.clear(); status_label->fg_role = 1; status_label->set(msg); }
    void start_busy(const std::string& msg) { busy = true; busy_msg = msg; status_label->fg_role = 3; status_label->set(msg); status_label->redraw(); }
    void stop_busy(const std::string& msg) { busy = false; busy_msg.clear(); status_label->fg_role = 3; status_label->set(msg); status_label->redraw(); }

    void ensure_bg_image() {
        if (bg_image || !home_shared_img || !scroll || scroll->w() < 10 || scroll->h() < 10) return;
        bg_image = new BgImage(scroll->x(), scroll->y(), scroll->w(), scroll->h());
        bg_image->set_image(home_shared_img); bg_image->show(); scroll->add(bg_image); scroll->redraw();
    }

    void layout(int W, int H) {
        int rx = SIDE_W + 10;
        search_input->resize(SIDE_W + 70, 55, W - SIDE_W - 220, 30);
        search_btn->resize(W - 130, 55, 100, 30);
        theme_choice->resize(W - 130, 12, 120, 26);
        back_btn->resize(W - 290, 12, 90, 26);
        scroll->resize(rx, 100, W - rx - 10, H - 150);
        if (bg_image) bg_image->resize(scroll->x(), scroll->y(), scroll->w(), scroll->h());
        int bar_y = H - 34, bar_h = 28;
        clean_btn->resize(W - 120, bar_y, 100, bar_h);
        fav_btn->resize(W - 120 - 10 - 170, bar_y, 170, bar_h);
        play_btn->resize(W - 120 - 10 - 170 - 10 - 130, bar_y, 130, bar_h);
        status_label->resize(SIDE_W + 10, bar_y, W - SIDE_W - 380, bar_h);
        fav_side_btn->resize(10, 60, SIDE_W - 20, 36);
        if (fav_count_label) fav_count_label->resize(10, 100, SIDE_W - 20, 16);
        if (!cells.empty()) rebuild_grid_positions_only();
        else if (!animes.empty()) rebuild_grid();
        else ensure_bg_image();
        if (bg_image) { bg_image->resize(scroll->x(), scroll->y(), scroll->w(), scroll->h()); bg_image->redraw(); bg_image->damage(FL_DAMAGE_ALL); }
        scroll->redraw(); scroll->damage(FL_DAMAGE_ALL); window->redraw(); window->damage(FL_DAMAGE_ALL);
    }

    void apply_colors() {
        window->color(C_BG); search_input->color(C_PANEL); search_input->textcolor(C_TEXT); search_input->selection_color(C_SEL);
        theme_choice->color(C_PANEL); theme_choice->textcolor(C_TEXT); theme_choice->selection_color(C_SEL);
        scroll->color(C_BG); scroll->redraw();
        play_btn->bg = C_BTN; play_btn->fg = C_TEXT; play_btn->hi = C_BTN_HI;
        fav_btn->bg = C_BTN; fav_btn->fg = C_TEXT; fav_btn->hi = C_BTN_HI;
        clean_btn->bg = C_BTN; clean_btn->fg = C_TEXT; clean_btn->hi = C_BTN_HI;
        search_btn->bg = C_ACCENT; search_btn->fg = C_BG; search_btn->hi = C_SEL;
        back_btn->bg = C_PANEL; back_btn->fg = C_TEXT; back_btn->hi = C_BTN_HI;
        fav_side_btn->bg = C_PANEL; fav_side_btn->fg = C_ACCENT; fav_side_btn->hi = C_BTN_HI;
        if (fav_count_label) fav_count_label->redraw();
        if (bg_image) bg_image->redraw();
        for (auto* c : cells) c->redraw();
        scroll->redraw(); window->redraw();
    }

    void rebuild_grid();
    void rebuild_grid_positions_only();
    void highlight_cell(int idx) {
        for (size_t i = 0; i < cells.size(); ++i) {
            Fl_Widget* w = cells[i];
            auto it = cell_kind.find(w);
            if (it == cell_kind.end()) continue;
            if (it->second == 0) {
                ((AnimeCell*)w)->set_selected((int)i == idx);
            } else if (it->second == 1) {
                ((ChapterRow*)w)->set_selected((int)i == idx);
            }
        }
    }
    void clear_cells() { for (auto* c : cells) delete c; cells.clear(); cell_kind.clear(); scroll->redraw(); scroll->damage(FL_DAMAGE_ALL); }

    static void cb_search(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->do_search(); }
    static void cb_play(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->do_play(); }
    static void cb_clean(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->do_clean(); }
    static void cb_fav(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->toggle_favorite(); }
    static void cb_show_favs(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->show_favorites(); }
    static void cb_back(Fl_Widget*, void* d) { ((RamenTVGUI*)d)->go_back(); }
    static void cb_theme(Fl_Widget*, void* d) {
        auto* self = (RamenTVGUI*)d;
        g_theme_index = self->theme_choice->value();
        apply_theme(g_theme_index); save_theme(g_theme_index);
        if (!self->animes.empty()) self->rebuild_grid();
    }
    static void cb_cell_clicked(Fl_Widget* w, void* d) {
        auto* cell = (AnimeCell*)w;
        auto* gui = (RamenTVGUI*)d;
        gui->selected_index = cell->index;
        gui->highlight_cell(cell->index);
        gui->play_btn->activate(); gui->fav_btn->activate();
        bool is_fav = Library::instance().is_favorite(gui->animes[cell->index].id);
        gui->fav_btn->label(is_fav ? "Quitar de favoritos" : "Favoritos");
        gui->fav_btn->redraw();
        gui->set_status("Seleccionado: " + gui->animes[cell->index].title);
        const std::string& _id = gui->animes[cell->index].id;
        if (_id.rfind("chapter_", 0) != 0 && _id.rfind("server_", 0) != 0) {
            gui->current_anime_id = _id;
            gui->current_anime_slug = gui->animes[cell->index].slug;
            gui->current_anime_title = gui->animes[cell->index].title;
        }
    }
    static void cb_chapter_clicked(Fl_Widget* w, void* d) {
        auto* gui = (RamenTVGUI*)d;
        auto it = gui->cell_kind.find(w);
        if (it == gui->cell_kind.end()) return;
        int idx = -1;
        if (it->second == 0)      idx = ((AnimeCell*)w)->index;
        else if (it->second == 1) idx = ((ChapterRow*)w)->index;
        if (idx < 0) return;
        gui->selected_index = idx;
        gui->highlight_cell(idx);
        gui->selected_chapter = idx + 1;
        gui->play_btn->activate();
        gui->set_status("Capítulo " + std::to_string(gui->selected_chapter) +
                        " seleccionado. Pulsa Reproducir.");
    }

    void update_fav_count() {
        if (!fav_count_label) return;
        int n = (int)Library::instance().favorites.size();
        char buf[32]; snprintf(buf, sizeof buf, "%d anime%s", n, n == 1 ? "" : "s");
        fav_count_label->set(buf); fav_count_label->redraw();
    }
    void save_state() { prev_view = current_view; prev_animes = animes; prev_query = search_input->value() ? search_input->value() : ""; back_btn->activate(); }
    void go_back() {
        if (prev_view == current_view && prev_animes.empty() && current_view != V_CHAPTERS && current_view != V_SERVERS) return;
        View target = prev_view;
        if (current_view == V_SERVERS) target = V_CHAPTERS;
        else if (current_view == V_CHAPTERS) target = (prev_view == V_SEARCH || prev_view == V_FAVS) ? prev_view : V_HOME;
        current_view = target; clear_cells(); animes = prev_animes; servers.clear();
        selected_index = -1; selected_chapter = -1; play_btn->deactivate(); fav_btn->deactivate();
        if (!prev_query.empty()) search_input->value(prev_query.c_str()); else search_input->value("");
        rebuild_grid(); if (current_view == V_HOME) back_btn->deactivate();
        set_status("Volviste a la vista anterior.");
    }
    void toggle_favorite() {
        if (current_view == V_CHAPTERS || current_view == V_SERVERS) { set_status("Vuelve a la lista para gestionar favoritos."); return; }
        if (selected_index < 0 || selected_index >= (int)animes.size()) { set_status("Selecciona un anime primero."); return; }
        const AnimeItem& a = animes[selected_index]; auto& lib = Library::instance();
        bool was_fav = lib.is_favorite(a.id);
        if (was_fav) {
            lib.remove_favorite(a.id); set_status("Quitado de favoritos: " + a.title); update_fav_count();
            if (current_view == V_FAVS) {
                animes.erase(animes.begin() + selected_index); selected_index = -1; play_btn->deactivate(); fav_btn->deactivate();
                rebuild_grid(); if (animes.empty()) set_status("No quedan favoritos.");
            } else { fav_btn->label("Favoritos"); fav_btn->redraw(); }
        } else {
            lib.add_favorite(a); set_status("Guardado en favoritos: " + a.title);
            fav_btn->label("Quitar de favoritos"); fav_btn->redraw(); update_fav_count();
        }
    }
    void show_favorites() {
        auto favs = Library::instance().favorites;
        if (favs.empty()) { set_status("No hay favoritos guardados."); return; }
        save_state(); current_view = V_FAVS; clear_cells(); animes = favs;
        selected_index = -1; play_btn->deactivate(); fav_btn->deactivate();
        rebuild_grid(); set_status(std::to_string(animes.size()) + " favoritos");
    }

    static void poll_results_cb(void* data) {
        auto* self = (RamenTVGUI*)data; self->poll_results();
        if (self->window && self->window->visible()) Fl::repeat_timeout(0.1, poll_results_cb, self);
    }
    void poll_results() {
        {
            SearchResult r;
            { std::lock_guard<std::mutex> lk(result_mutex); if (!result_ready) goto check_chapters; r = pending_result; result_ready = false; }
            searching = false;
            if (!r.ok) {
                std::string msg = "search.sh no genero search.json.";
                if (!r.log.empty()) {
                    size_t p = 0; while (p < r.log.size() && (r.log[p]=='\n' || r.log[p]=='\r')) ++p;
                    size_t e = r.log.find('\n', p); msg += " " + r.log.substr(p, e == std::string::npos ? 100 : e - p);
                }
                stop_busy(msg); return;
            }
            animes = parse_search_json(r.json);
            if (animes.empty()) { stop_busy("Sin resultados para: " + r.query); return; }
            clear_cells(); selected_index = -1; play_btn->deactivate(); fav_btn->deactivate();
            rebuild_grid(); scroll->scroll_to(0, 0);
            stop_busy(std::to_string(animes.size()) + " animes para: " + r.query); return;
        }
    check_chapters:
        {
            ChaptersResult r;
            { std::lock_guard<std::mutex> lk(result_mutex); if (!chapters_ready) goto check_servers; r = pending_chapters; chapters_ready = false; }
            if (!r.ok || r.total <= 0) { stop_busy("No se pudo obtener capítulos. " + r.log); return; }
            show_chapters_grid(r.total); stop_busy("Anime con " + std::to_string(r.total) + " capítulos. Selecciona uno y pulsa Reproducir."); return;
        }
    check_servers:
        {
            ServersResult r;
            { std::lock_guard<std::mutex> lk(result_mutex); if (!servers_ready) return; r = pending_servers; servers_ready = false; }
            if (!r.ok) { stop_busy("No se encontraron servidores. " + r.log); return; }
            servers = parse_servers_json(r.json);
            if (servers.empty()) { stop_busy("servers.json vacío."); return; }
            show_servers_list(); stop_busy(std::to_string(servers.size()) + " servidores disponibles."); return;
        }
    }

    void show_chapters_grid(int total) {
        clear_cells();
        if (bg_image) bg_image->hide();
        animes.clear();
        int limit = std::min(total, 500);
        for (int i = 1; i <= limit; ++i) {
            AnimeItem a;
            a.id = "chapter_" + std::to_string(i);
            a.title = "Capítulo " + std::to_string(i);
            a.year = ""; a.imdb = "";
            animes.push_back(a);
        }
        const int ROW_H = 44, ROW_GAP = 6, PAD = PADDING;
        int list_w = scroll->w() - 2 * PAD;
        if (list_w > 700) list_w = 700;
        int offset_x = scroll->x() + (scroll->w() - list_w) / 2;
        int base_y = scroll->y() + PAD;
        for (size_t i = 0; i < animes.size(); ++i) {
            auto* row = new ChapterRow(offset_x, base_y + (int)i * (ROW_H + ROW_GAP),
                                       list_w, ROW_H, &animes[i], (int)i);
            row->callback(cb_chapter_clicked, this);
            row->when(FL_WHEN_RELEASE);
            scroll->add(row);
            cells.push_back(row); cell_kind[row] = 1;
        }
        int total_h = PAD * 2 + (int)animes.size() * (ROW_H + ROW_GAP);
        scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, total_h);
        scroll->add(scroll_spacer);
        scroll->scroll_to(0, 0);
        scroll->redraw();
        scroll->damage(FL_DAMAGE_ALL);
        window->redraw();
    }

    void show_servers_list() {
        clear_cells(); if (bg_image) bg_image->hide(); animes.clear();
        for (size_t i = 0; i < servers.size(); ++i) {
            AnimeItem a; a.id = "server_" + std::to_string(i); a.title = servers[i].name; a.year = ""; a.imdb = servers[i].url.substr(0, 60);
            animes.push_back(a);
        }
        int usable_w = scroll->w() - 2 * PADDING, cols = usable_w / (CELL_W + GAP);
        if (cols < 2) cols = 2;
        int grid_w = cols * CELL_W + (cols - 1) * GAP, offset_x = scroll->x() + (scroll->w() - grid_w) / 2, base_y = scroll->y() + PADDING;
        for (size_t i = 0; i < animes.size(); ++i) {
            int col = (int)i % cols, row = (int)i / cols;
            auto* cell = new AnimeCell(offset_x + col * (CELL_W + GAP), base_y + row * (CELL_H + GAP), CELL_W, CELL_H, &animes[i], (int)i);
            cell->callback(cb_chapter_clicked, this); cell->when(FL_WHEN_RELEASE); // Reutilizamos cb_chapter_clicked para activar reproducción
            scroll->add(cell); cells.push_back(cell); cell_kind[cell] = 0;
        }
        int rows_count = ((int)animes.size() + cols - 1) / cols;
        scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, PADDING * 2 + rows_count * CELL_H + (rows_count - 1) * GAP);
        scroll->add(scroll_spacer); scroll->scroll_to(0, 0); scroll->redraw(); scroll->damage(FL_DAMAGE_ALL); window->redraw();
    }

    void do_search();
    void do_play();
    void do_clean();
    void play_with_mplayer(const std::string& m3u8_url, const std::string& title, const std::string& chap_num);
    void download_posters_sync(const std::vector<size_t>& pending);
};

void RamenTVGUI::download_posters_sync(const std::vector<size_t>& pending) {
    if (pending.empty()) return;
    std::system("mkdir -p /tmp/TEMP_A-CLI");
    const int GROUP = 4;
    for (size_t g = 0; g < pending.size(); g += GROUP) {
        size_t end = std::min(pending.size(), g + GROUP);
        std::vector<std::thread> workers;
        for (size_t k = g; k < end; ++k) {
            size_t idx = pending[k];
            workers.emplace_back([this, idx]() {
                const AnimeItem& a = animes[idx];
                std::string out = "/tmp/TEMP_A-CLI/poster_" + a.id + ".img";
                struct stat st;
                if (stat(out.c_str(), &st) == 0 && st.st_size > 1000) return;
                if (!a.img.empty()) {
                    std::string cmd = "wget -q --no-check-certificate --timeout=8 --tries=1 -O " + shell_quote(out) + " " + shell_quote(a.img) + " 2>/dev/null";
                    std::system(cmd.c_str());
                }
                if (stat(out.c_str(), &st) != 0 || st.st_size <= 1000) std::remove(out.c_str());
            });
        }
        for (auto& t : workers) t.join();
    }
}

void RamenTVGUI::rebuild_grid_positions_only() {
    if (cells.empty() || animes.empty()) return;
    if (current_view == V_CHAPTERS) {
        const int ROW_H = 44, ROW_GAP = 6, PAD = PADDING;
        int list_w = scroll->w() - 2 * PAD;
        if (list_w > 700) list_w = 700;
        int offset_x = scroll->x() + (scroll->w() - list_w) / 2;
        int base_y = scroll->y() + PAD;
        for (size_t i = 0; i < cells.size(); ++i) {
            cells[i]->resize(offset_x, base_y + (int)i * (ROW_H + ROW_GAP), list_w, ROW_H);
            cells[i]->redraw();
        }
        int total_h = PAD * 2 + (int)animes.size() * (ROW_H + ROW_GAP);
        scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, total_h);
        scroll->redraw();
        scroll->damage(FL_DAMAGE_ALL);
        return;
    }
    int usable_w = scroll->w() - 2 * PADDING, cols = usable_w / (CELL_W + GAP);
    if (cols < 2) cols = 2;
    int grid_w = cols * CELL_W + (cols - 1) * GAP, offset_x = scroll->x() + (scroll->w() - grid_w) / 2, base_y = scroll->y() + PADDING;
    for (size_t i = 0; i < cells.size(); ++i) {
        int col = (int)i % cols, row = (int)i / cols;
        cells[i]->resize(offset_x + col * (CELL_W + GAP), base_y + row * (CELL_H + GAP), CELL_W, CELL_H);
        cells[i]->redraw();
    }
    int rows_count = ((int)animes.size() + cols - 1) / cols;
    scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, PADDING * 2 + rows_count * CELL_H + (rows_count - 1) * GAP);
    scroll->redraw(); scroll->damage(FL_DAMAGE_ALL);
}

void RamenTVGUI::rebuild_grid() {
    clear_cells();
    std::vector<size_t> pending;
    for (size_t i = 0; i < animes.size(); ++i) {
        if (animes[i].img.empty()) continue;
        std::string path = "/tmp/TEMP_A-CLI/poster_" + animes[i].id + ".img";
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && st.st_size > 20000) continue;
        if (stat(path.c_str(), &st) == 0) std::remove(path.c_str());
        pending.push_back(i);
    }
    download_posters_sync(pending);
    if (animes.empty()) {
        scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, 10);
        if (bg_image) { bg_image->show(); bg_image->redraw(); }
        scroll->redraw(); return;
    }
    if (bg_image) bg_image->hide();
    int usable_w = scroll->w() - 2 * PADDING, cols = usable_w / (CELL_W + GAP);
    if (cols < 2) cols = 2;
    int grid_w = cols * CELL_W + (cols - 1) * GAP, offset_x = scroll->x() + (scroll->w() - grid_w) / 2, base_y = scroll->y() + PADDING;
    for (size_t i = 0; i < animes.size(); ++i) {
        int col = (int)i % cols, row = (int)i / cols;
        auto* cell = new AnimeCell(offset_x + col * (CELL_W + GAP), base_y + row * (CELL_H + GAP), CELL_W, CELL_H, &animes[i], (int)i);
        cell->callback(cb_cell_clicked, this); cell->when(FL_WHEN_RELEASE);
        scroll->add(cell); cells.push_back(cell); cell_kind[cell] = 0;
    }
    int rows_count = ((int)animes.size() + cols - 1) / cols;
    scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, PADDING * 2 + rows_count * CELL_H + (rows_count - 1) * GAP);
    scroll->add(scroll_spacer); scroll->scroll_to(0, 0); scroll->redraw(); scroll->damage(FL_DAMAGE_ALL);
    window->redraw(); window->damage(FL_DAMAGE_ALL); Fl::flush();
}

void RamenTVGUI::do_search() {
    if (searching) { set_status("Búsqueda en curso..."); return; }
    std::string q = search_input->value() ? search_input->value() : "";
    if (q.empty()) { set_status("Escribe un término de búsqueda."); return; }
    struct stat st;
    if (stat(cli_dir.c_str(), &st) != 0) { set_status("No existe el directorio: " + cli_dir); return; }
    if (stat((cli_dir + "/search.sh").c_str(), &st) != 0) { set_status("No existe: " + cli_dir + "/search.sh"); return; }
    if (current_view != V_SEARCH) save_state();
    current_view = V_SEARCH; searching = true; start_busy("Buscando: " + q);
    std::string qq = q; for (auto& c : qq) if (c=='\''||c=='"'||c=='\\'||c=='`') c = ' ';
    std::string cli = cli_dir;
    { std::lock_guard<std::mutex> lk(result_mutex); result_ready = false; pending_result = SearchResult{}; }
    std::thread([this, qq, cli]() {
        std::system("mkdir -p /tmp/TEMP_A-CLI"); std::system("rm -f /tmp/TEMP_A-CLI/search.json");
        std::string cmd = "cd " + shell_quote(cli) + " && bash search.sh " + shell_quote(qq) + " 2>&1";
        std::string log = run_cmd_timeout(cmd, 120);
        { std::ofstream lf("/tmp/TEMP_A-CLI/search_gui.log"); lf << "CMD: " << cmd << "\n---\n" << log << "\n"; }
        bool ok = file_exists("/tmp/TEMP_A-CLI/search.json");
        std::string json = ok ? read_file("/tmp/TEMP_A-CLI/search.json") : "";
        {
            std::lock_guard<std::mutex> lk(result_mutex);
            pending_result.json = std::move(json); pending_result.log = std::move(log);
            pending_result.query = qq; pending_result.ok = ok; result_ready = true;
        }
    }).detach();
}

void RamenTVGUI::do_play() {
    if (current_view == V_SEARCH || current_view == V_FAVS) {
        if (selected_index < 0 || selected_index >= (int)animes.size()) { set_status("Selecciona un anime primero."); return; }
        const AnimeItem& a = animes[selected_index];
        if (a.id.empty()) { set_status("Anime sin ID."); return; }
        struct stat st;
        if (stat((cli_dir + "/chapters.sh").c_str(), &st) != 0) { set_status("No existe: " + cli_dir + "/chapters.sh"); return; }
        save_state(); current_view = V_CHAPTERS; current_anime_id = a.id; start_busy("Obteniendo capítulos de: " + a.title);
        std::string id = a.id, cli = cli_dir;
        { std::lock_guard<std::mutex> lk(result_mutex); chapters_ready = false; pending_chapters = ChaptersResult{}; }
        std::thread([this, id, cli]() {
            std::string cmd = "cd " + shell_quote(cli) + " && bash chapters.sh " + shell_quote(id) + " 2>&1";
            std::string log = run_cmd_timeout(cmd, 60); std::string out = trim(log); int total = 0;
            std::istringstream iss(out); std::string line;
            while (std::getline(iss, line)) { line = trim(line); if (line.empty()) continue; try { total = std::stoi(line); } catch (...) {} }
            {
                std::lock_guard<std::mutex> lk(result_mutex);
                pending_chapters.total = total; pending_chapters.log = out; pending_chapters.ok = (total > 0); chapters_ready = true;
            }
        }).detach();
        return;
    }

    // Caso 2: estamos en V_CHAPTERS -> Reproducción automática con el primer servidor y mplayer
    if (current_view == V_CHAPTERS) {
        if (selected_chapter <= 0) {
            set_status("Selecciona un capítulo primero.");
            return;
            }
        if (current_anime_slug.empty()) {
            set_status("Error interno: falta el slug del anime.");
            return;
        }

        start_busy("Obteniendo enlace de reproducción...");

        std::string slug = this->current_anime_slug;
        std::string chap_num = std::to_string(selected_chapter);
        std::string cli = cli_dir;
        std::string title = this->current_anime_title.empty() ? "Anime" : this->current_anime_title;

        std::thread([this, slug, chap_num, cli, title]() {
            std::string chapter_url = "https://jkanime.net/" + slug + "/" + chap_num + "/";
            std::string html_path = "/tmp/TEMP_A-CLI/anime.html";
            
            // 1. Descargar HTML del capítulo
            std::string dl_cmd = "curl -s -L -A 'Mozilla/5.0 (X11; Linux x86_64; rv:146.0) Gecko/20100101 Firefox/146.0' "
                                 "-b /tmp/TEMP_A-CLI/cookies.txt -o " + shell_quote(html_path) + " " + shell_quote(chapter_url) + " 2>&1";
            run_cmd_timeout(dl_cmd, 30);

            if (!file_exists(html_path)) {
                Fl::add_timeout(0.05, [](void* d) { ((RamenTVGUI*)d)->stop_busy("Error al descargar la página del capítulo."); }, this);
                return;
            }

            // 2. Ejecutar servers.sh
            std::string cmd_servers = "cd " + shell_quote(cli) + " && bash servers.sh 2>&1";
            run_cmd_timeout(cmd_servers, 60);

            std::string servers_json = read_file("/tmp/TEMP_A-CLI/servers.json");
            auto servers = parse_servers_json(servers_json);
            if (servers.empty()) {
                Fl::add_timeout(0.05, [](void* d) { ((RamenTVGUI*)d)->stop_busy("No se encontraron servidores."); }, this);
                return;
            }

            // 3. Tomar el primer servidor (jkplayer/um) automáticamente
            std::string first_server = servers[0].url;
            fprintf(stderr, "[RamenTV] Usando servidor principal: %s\n", first_server.c_str());

            // 4. Ejecutar m3u8.sh
            std::string cmd_m3u8 = "cd " + shell_quote(cli) + " && bash m3u8.sh " + shell_quote(first_server) + " 2>&1";
            run_cmd_timeout(cmd_m3u8, 120);

            std::string m3u8_content = trim(read_file("/tmp/TEMP_A-CLI/stream.m3u8"));
            if (m3u8_content.empty()) {
                Fl::add_timeout(0.05, [](void* d) { ((RamenTVGUI*)d)->stop_busy("No se pudo extraer el enlace M3U8."); }, this);
                return;
            }

            // 5. Reproducir con yt-dlp + mplayer
            PlayData* pd = new PlayData{this, m3u8_content, title, chap_num};
            Fl::add_timeout(0.05, [](void* data) {
                PlayData* p = (PlayData*)data;
                p->gui->play_with_mplayer(p->m3u8, p->title, p->chap);
                delete p;
            }, pd);
        }).detach();
        return;
    }
}

void RamenTVGUI::play_with_mplayer(const std::string& m3u8_url, const std::string& title, const std::string& chap_num) {
    if (m3u8_url.empty()) { set_status("URL de stream vacía."); return; }
    start_busy(title);

    std::thread([this, m3u8_url, title, chap_num]() {
        std::string which_ytdlp = run_cmd_timeout("command -v yt-dlp 2>/dev/null", 5);
        std::string which_mplayer = run_cmd_timeout("command -v mplayer 2>/dev/null", 5);
        
        if (which_ytdlp.empty() || which_mplayer.empty()) {
            Fl::add_timeout(0.05, [](void* d) {
                ((RamenTVGUI*)d)->stop_busy("yt-dlp o mplayer no encontrados. Instálalos.");
            }, this);
            return;
        }

        auto esc = [](std::string s) {
            std::string o;
            for (char c : s) {
                if (c == '"' || c == '\\' || c == '$' || c == '`') o += '\\';
                o += c;
            }
            return o;
        };

        std::string safe_title = title + " - Capitulo " + chap_num;
        for (auto& c : safe_title) if (c == '"' || c == '\\') c = ' ';

        std::string cmd = "( cd /tmp && yt-dlp --add-header \"Referer: https://jkanime.net\" "
                          "-S \"res:480\" -o - " + shell_quote(m3u8_url) + " "
                          "2>/tmp/TEMP_A-CLI/ytdlp.log ) | "
                          "mplayer -zoom -vf scale=640:480 -cache 6144 -demuxer lavf "
                          "-title \"" + esc(safe_title) + "\" - "
                          ">/tmp/TEMP_A-CLI/mplayer.log 2>&1";
        
        std::system(cmd.c_str());
        
        Fl::add_timeout(0.05, [](void* d) {
            ((RamenTVGUI*)d)->stop_busy("Reproducción finalizada.");
        }, this);
    }).detach();
}

void RamenTVGUI::do_clean() {
    clear_cells(); animes.clear(); servers.clear();
    selected_index = -1; selected_chapter = -1; current_anime_id.clear(); current_anime_slug.clear(); current_anime_title.clear(); current_view = V_HOME;
    play_btn->deactivate(); fav_btn->deactivate();
    scroll_spacer->resize(scroll->x(), scroll->y(), scroll->w() - 2, 10);
    if (bg_image) { bg_image->show(); bg_image->redraw(); }
    scroll->redraw(); window->redraw(); set_status("Limpiando archivos...");
    std::thread([this]() {
        std::system("rm -f /tmp/TEMP_A-CLI/*.json /tmp/TEMP_A-CLI/*.html /tmp/TEMP_A-CLI/*.m3u8 /tmp/TEMP_A-CLI/*.log /tmp/TEMP_A-CLI/*.img 2>/dev/null");
        Fl::add_timeout(0.05, [](void* data) { ((RamenTVGUI*)data)->set_status("Todo limpio."); }, this);
    }).detach();
}

int main(int argc, char** argv) {
    {
        char cwd[PATH_MAX] = {0}; getcwd(cwd, sizeof cwd);
        if (std::system("command -v jq >/dev/null 2>&1") != 0) fprintf(stderr, "[RamenTV] AVISO: jq no esta en PATH\n");
        if (std::system("command -v yt-dlp >/dev/null 2>&1") != 0) fprintf(stderr, "[RamenTV] AVISO: yt-dlp no esta en PATH\n");
        if (std::system("command -v mplayer >/dev/null 2>&1") != 0) fprintf(stderr, "[RamenTV] AVISO: mplayer no esta en PATH\n");
        fprintf(stderr, "[RamenTV] cwd: %s\n", cwd);
        if (const char* env = std::getenv("RAMENTV_HOME")) fprintf(stderr, "[RamenTV] RAMENTV_HOME=%s\n", env);
    }
    const int win_w = 900, win_h = 600;
    g_theme_index = load_saved_theme(); Library::instance().load();
    RamenTVGUI gui(win_w, win_h, "RamenTV FLTK");
    gui.window->size_range(700, 500); gui.show();
    apply_theme(g_theme_index); gui.apply_colors(); gui.layout(win_w, win_h);
    Fl::add_timeout(0.1, [](void* g) { auto* ggui = (RamenTVGUI*)g; ggui->layout(ggui->window->w(), ggui->window->h()); }, &gui);
    return Fl::run();
}
