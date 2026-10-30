#include "library.h"
#include "cJSON.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>

Library& Library::instance() { 
    static Library lib; 
    return lib; 
}

static std::string data_path() {
    const char* home = getenv("HOME");
    std::string dir = std::string(home ? home : "/tmp") + "/.config/ramentv-fltk";
    mkdir(dir.c_str(), 0700);
    return dir + "/data.json";
}

static AnimeItem item_from_json(cJSON* o) {
    AnimeItem it; 
    cJSON* v;
    
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "id")) && cJSON_IsString(v)) it.id = v->valuestring;
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "slug")) && cJSON_IsString(v)) it.slug = v->valuestring;
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "title")) && cJSON_IsString(v)) it.title = v->valuestring;
    
    // Mapeo de imágenes
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "img")) && cJSON_IsString(v)) it.img = v->valuestring;
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "thumbnail")) && cJSON_IsString(v)) it.thumbnail = v->valuestring;
    
    // Nuevos campos para la GUI
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "url")) && cJSON_IsString(v)) it.url = v->valuestring;
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "year")) && cJSON_IsString(v)) it.year = v->valuestring;
    if ((v = cJSON_GetObjectItemCaseSensitive(o, "imdb")) && cJSON_IsString(v)) it.imdb = v->valuestring;
    
    return it;
}

static cJSON* item_to_json(const AnimeItem& it) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", it.id.c_str());
    cJSON_AddStringToObject(o, "slug", it.slug.c_str());
    cJSON_AddStringToObject(o, "title", it.title.c_str());
    cJSON_AddStringToObject(o, "img", it.img.c_str());
    cJSON_AddStringToObject(o, "thumbnail", it.thumbnail.c_str());
    
    // Guardar nuevos campos si no están vacíos
    if (!it.url.empty()) cJSON_AddStringToObject(o, "url", it.url.c_str());
    if (!it.year.empty()) cJSON_AddStringToObject(o, "year", it.year.c_str());
    if (!it.imdb.empty()) cJSON_AddStringToObject(o, "imdb", it.imdb.c_str());
    
    return o;
}

void Library::load() {
    favorites.clear();
    std::ifstream f(data_path());
    if (!f.is_open()) return;
    
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    
    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) return;
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return; }
    
    cJSON* favs = cJSON_GetObjectItemCaseSensitive(root, "favorites");
    cJSON* item = nullptr;
    
    cJSON_ArrayForEach(item, favs) {
        AnimeItem it = item_from_json(item);
        if (!it.title.empty()) favorites.push_back(it);
    }
    
    cJSON_Delete(root);
}

void Library::save() {
    cJSON* root = cJSON_CreateObject();
    cJSON* favs = cJSON_CreateArray();
    
    for (auto& it : favorites)
        cJSON_AddItemToArray(favs, item_to_json(it));
        
    cJSON_AddItemToObject(root, "favorites", favs);
    
    char* str = cJSON_PrintUnformatted(root);
    std::ofstream f(data_path());
    if (f.is_open()) f << str;
    f.close();
    
    free(str);
    cJSON_Delete(root);
}

void Library::add_favorite(const AnimeItem& it) {
    if (!is_favorite(it.id)) { 
        favorites.push_back(it); 
        save(); 
    }
}

void Library::remove_favorite(const std::string& id) {
    favorites.erase(
        std::remove_if(favorites.begin(), favorites.end(),
                       [&](const AnimeItem& m){ return m.id == id; }),
        favorites.end());
    save();
}

bool Library::is_favorite(const std::string& id) const {
    for (auto& m : favorites) 
        if (m.id == id) return true;
    return false;
}
