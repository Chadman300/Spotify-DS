#include "spotify_client.hpp"

SpotifyClient::SpotifyClient()
    : config_(SpotifyConfigStore::load())
    , authRequest_(SpotifyAuth::createRequest(config_))
    , session_()
    , api_(session_) {
}

SpotifyConfig& SpotifyClient::config() {
    return config_;
}

const SpotifyConfig& SpotifyClient::config() const {
    return config_;
}

void SpotifyClient::refreshAuthRequest() {
    authRequest_ = SpotifyAuth::createRequest(config_);
}

bool SpotifyClient::saveConfig() {
    return SpotifyConfigStore::save(config_);
}

bool SpotifyClient::isSignedIn() const {
    return session_.isAuthenticated();
}

const std::string& SpotifyClient::profileName() const {
    return session_.profileName();
}

std::string SpotifyClient::authorizationUrl() const {
    return SpotifyAuth::buildAuthorizationUrl(authRequest_);
}

bool SpotifyClient::signIn(const std::string& code, std::string* error) {
    if (!api_.exchangeCodeForToken(authRequest_, code, error)) {
        return false;
    }

    return api_.loadProfile(error);
}

void SpotifyClient::signOut() {
    session_.clear();
}

bool SpotifyClient::loadProfile(std::string* error) {
    return api_.loadProfile(error);
}

std::vector<TrackItem> SpotifyClient::searchTracks(const std::string& query, std::string* error) {
    return api_.searchTracks(query, error);
}

std::vector<PlaylistItem> SpotifyClient::loadPlaylists(std::string* error) {
    return api_.loadPlaylists(error);
}

std::vector<TrackItem> SpotifyClient::loadPlaylistTracks(const std::string& playlistId, std::string* error) {
    return api_.loadPlaylistTracks(playlistId, error);
}

bool SpotifyClient::loadNowPlaying(TrackItem* track, bool* isPlaying, std::string* error) {
    return api_.loadNowPlaying(track, isPlaying, error);
}

bool SpotifyClient::playTrack(const TrackItem& track, std::string* error) {
    return api_.playTrack(track, error);
}

bool SpotifyClient::queueTrack(const TrackItem& track, std::string* error) {
    return api_.queueTrack(track, error);
}
