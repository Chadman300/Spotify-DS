#pragma once

#include <string>
#include <vector>

struct TrackItem {
    std::string id;
    std::string name;
    std::string artist;
};

struct PlaylistItem {
    std::string id;
    std::string name;
    std::string description;
};

enum class AppView {
    Home,
    Search,
    Playlists,
    PlaylistDetail,
    NowPlaying,
    SignIn,
};

struct AppState {
    std::string statusMessage;
    std::string authUrl;
    AppView view = AppView::Home;
    bool isSignedIn = false;
    bool hasNowPlaying = false;
    bool isPlaying = false;
    std::string activeProfileName;
    std::string searchQuery;
    std::vector<TrackItem> searchResults;
    std::vector<PlaylistItem> playlistResults;
    std::vector<TrackItem> playlistTracks;
    TrackItem nowPlaying;
    PlaylistItem selectedPlaylist;
    int selectedIndex = 0;
};
