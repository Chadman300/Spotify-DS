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
    std::string clientId = "replace-with-your-spotify-client-id";
    std::string redirectUri = "spotifyds://callback";
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