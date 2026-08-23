#pragma once

#include "spotify_config.hpp"

#include <string>

struct SpotifyAuthRequest {
    std::string clientId;
    std::string redirectUri;
    std::string scope;
    std::string codeVerifier;
    std::string codeChallenge;
};

class SpotifyAuth {
public:
    static SpotifyAuthRequest createRequest(const SpotifyConfig& config = SpotifyConfig());
    static std::string buildAuthorizationUrl(const SpotifyAuthRequest& request);
    static std::string buildTokenExchangeBody(const SpotifyAuthRequest& request, const std::string& code);
    static std::string buildRefreshBody(const SpotifyAuthRequest& request, const std::string& refreshToken);
};