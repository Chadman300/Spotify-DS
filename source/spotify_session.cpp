#include "spotify_session.hpp"

bool SpotifySession::isAuthenticated() const {
    return !accessToken_.empty();
}

bool SpotifySession::hasRefreshToken() const {
    return !refreshToken_.empty();
}

void SpotifySession::setAccessToken(std::string token) {
    accessToken_ = std::move(token);
}

void SpotifySession::setTokens(std::string accessToken, std::string refreshToken) {
    accessToken_ = std::move(accessToken);
    refreshToken_ = std::move(refreshToken);
}

void SpotifySession::setProfileName(std::string profileName) {
    profileName_ = std::move(profileName);
}

void SpotifySession::clear() {
    accessToken_.clear();
    refreshToken_.clear();
    profileName_.clear();
}

const std::string& SpotifySession::accessToken() const {
    return accessToken_;
}

const std::string& SpotifySession::refreshToken() const {
    return refreshToken_;
}

const std::string& SpotifySession::profileName() const {
    return profileName_;
}
