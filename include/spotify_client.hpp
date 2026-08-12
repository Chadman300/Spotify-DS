#pragma once

#include <string>
#include <vector>

#include "app_state.hpp"
#include "spotify_api.hpp"
#include "spotify_config.hpp"

class SpotifyClient {
public:
    SpotifyClient();

    SpotifyConfig& config();
    const SpotifyConfig& config() const;
    bool isSignedIn() const;
    const std::string& profileName() const;
    std::string authorizationUrl() const;
    bool signIn(const std::string& code, std::string* error);
    void signOut();
    bool saveConfig();
    bool loadProfile(std::string* error);
    std::vector<TrackItem> searchTracks(const std::string& query, std::string* error);
    std::vector<PlaylistItem> loadPlaylists(std::string* error);
    std::vector<TrackItem> loadPlaylistTracks(const std::string& playlistId, std::string* error);
    bool loadNowPlaying(TrackItem* track, bool* isPlaying, std::string* error);
    bool playTrack(const TrackItem& track, std::string* error);
    bool queueTrack(const TrackItem& track, std::string* error);

private:
    void refreshAuthRequest();

    SpotifyConfig config_;
    SpotifyAuthRequest authRequest_;
    SpotifySession session_;
    SpotifyApi api_;
};
