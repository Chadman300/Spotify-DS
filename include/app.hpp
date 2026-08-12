#pragma once

#include "app_state.hpp"
#include "spotify_client.hpp"

#include <string>

class App {
public:
    App();
    int run();
    void signOut();

private:
    bool promptText(const std::string& title, const std::string& hint, std::string* outValue, int maxLength = 128, const std::string& initialValue = std::string());
    void refreshHome();
    void refreshPlaylists();
    void refreshPlaylistDetail(const PlaylistItem& playlist);
    void refreshNowPlaying();
    void runSearch();
    void signIn();
    void moveSelection(int delta);
    std::string friendlyError(const std::string& error) const;
    void render() const;
    const TrackItem* selectedTrack() const;
    const PlaylistItem* selectedPlaylist() const;

    AppState state_;
    SpotifyClient client_;
};
