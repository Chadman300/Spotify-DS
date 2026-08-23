#pragma once

#include <string>

enum class LastView {
    Home = 0,
    Search = 1,
    Playlists = 2,
    PlaylistDetail = 3,
    NowPlaying = 4,
    SignIn = 5,
};

struct SpotifyConfig {
    std::string clientId = "39313b7bfd4e4dae8d5853dfa1244e2a";
    std::string redirectUri = "https://www.budgetappco.com/spotify-callback";
    std::string scope = "user-read-playback-state user-modify-playback-state playlist-read-private playlist-read-collaborative user-read-private";
    std::string lastSearchQuery;
    std::string lastPlaylistId;
    LastView lastView = LastView::Home;
};

class SpotifyConfigStore {
public:
    static SpotifyConfig load();
    static bool save(const SpotifyConfig& config);
};