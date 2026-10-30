#ifndef RAMENTV_LIBRARY_H
#define RAMENTV_LIBRARY_H

#include <string>
#include <vector>

// Un anime guardado en favoritos o resultados de búsqueda
struct AnimeItem {
    std::string id;
    std::string slug;
    std::string title;
    std::string img;     // Usado como 'img' en la GUI
    std::string thumbnail;
    
    // Campos adicionales necesarios para la GUI de Palomitas/RamenTV
    std::string url;       // URL del capítulo/anime
    std::string year;      // Año de emisión
    std::string imdb;      // ID de IMDb
};

// Alias para compatibilidad con la GUI (Palomitas-GUI.cpp usa MovieItem)
typedef AnimeItem MovieItem;

struct Library {
    std::vector<AnimeItem> favorites;
    
    static Library& instance();
    void load();
    void save();
    void add_favorite(const AnimeItem& it);
    void remove_favorite(const std::string& id);
    bool is_favorite(const std::string& id) const;
};

#endif
