#pragma once

#include "app_state.hpp"
#include "spotify_auth.hpp"
#include "spotify_session.hpp"

#include <string>
#include <vector>

class SpotifyApi {
public:
    explicit SpotifyApi(SpotifySession& session);

    bool exchangeCodeForToken(const SpotifyAuthRequest& request, const std::string& code, std::string* error);
    bool refreshAccessToken(const SpotifyAuthRequest& request, std::string* error);
    bool loadProfile(std::string* error);
    std::vector<TrackItem> searchTracks(const std::string& query, std::string* error);
    std::vector<PlaylistItem> loadPlaylists(std::string* error);
    std::vector<TrackItem> loadPlaylistTracks(const std::string& playlistId, std::string* error);
    bool loadNowPlaying(TrackItem* track, bool* isPlaying, std::string* error);
    bool playTrack(const TrackItem& track, std::string* error);
    bool queueTrack(const TrackItem& track, std::string* error);

private:
    struct HttpResponse {
        int statusCode = 0;
        std::string body;
    };

    HttpResponse request(const std::string& method, const std::string& url, const std::vector<std::string>& headers, const std::string& body, const std::string& contentType = std::string(), std::string* error = nullptr) const;
    HttpResponse requestWithAuthRetry(const std::string& method, const std::string& url, const std::vector<std::string>& headers, const std::string& body, const std::string& contentType = std::string(), std::string* error = nullptr);
    static std::string urlEncode(const std::string& input);
    static std::string extractJsonString(const std::string& json, const std::string& key);
    static std::string extractJsonStringFrom(const std::string& json, size_t start);
    static bool extractJsonBool(const std::string& json, const std::string& key, bool defaultValue = false);
    static std::vector<TrackItem> parseTracks(const std::string& json);
    static std::vector<PlaylistItem> parsePlaylists(const std::string& json);
    static std::vector<TrackItem> parsePlaylistTracks(const std::string& json);

    SpotifySession& session_;
};