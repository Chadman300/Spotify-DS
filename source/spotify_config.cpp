#include "spotify_config.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr const char* kConfigPath = "sdmc:/3ds/Spotify-DS/config.txt";

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }
    return value.substr(start);
}

LastView parseLastView(const std::string& value) {
    if (value == "search") return LastView::Search;
    if (value == "playlists") return LastView::Playlists;
    if (value == "playlist_detail") return LastView::PlaylistDetail;
    if (value == "now_playing") return LastView::NowPlaying;
    if (value == "signin") return LastView::SignIn;
    return LastView::Home;
}

std::string lastViewToString(LastView view) {
    switch (view) {
        case LastView::Search: return "search";
        case LastView::Playlists: return "playlists";
        case LastView::PlaylistDetail: return "playlist_detail";
        case LastView::NowPlaying: return "now_playing";
        case LastView::SignIn: return "signin";
        case LastView::Home:
        default:
            return "home";
    }
}
}

SpotifyConfig SpotifyConfigStore::load() {
    SpotifyConfig config;

    std::FILE* file = std::fopen(kConfigPath, "r");
    if (!file) {
        return config;
    }

    char line[512];
    while (std::fgets(line, sizeof(line), file)) {
        std::string entry = trim(line);
        const size_t equals = entry.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        std::string key = trim(entry.substr(0, equals));
        std::string value = trim(entry.substr(equals + 1));

        if (key == "client_id") {
            config.clientId = value;
        } else if (key == "redirect_uri") {
            config.redirectUri = value;
        } else if (key == "scope") {
            config.scope = value;
        } else if (key == "last_search_query") {
            config.lastSearchQuery = value;
        } else if (key == "last_playlist_id") {
            config.lastPlaylistId = value;
        } else if (key == "last_view") {
            config.lastView = parseLastView(value);
        }
    }

    std::fclose(file);
    return config;
}

bool SpotifyConfigStore::save(const SpotifyConfig& config) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/Spotify-DS", 0777);

    std::FILE* file = std::fopen(kConfigPath, "w");
    if (!file) {
        return false;
    }

    std::fprintf(file, "client_id=%s\n", config.clientId.c_str());
    std::fprintf(file, "redirect_uri=%s\n", config.redirectUri.c_str());
    std::fprintf(file, "scope=%s\n", config.scope.c_str());
    std::fprintf(file, "last_search_query=%s\n", config.lastSearchQuery.c_str());
    std::fprintf(file, "last_playlist_id=%s\n", config.lastPlaylistId.c_str());
    std::fprintf(file, "last_view=%s\n", lastViewToString(config.lastView).c_str());

    std::fclose(file);
    return true;
}