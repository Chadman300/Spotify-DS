#pragma once

#include <string>

class SpotifySession {
public:
    bool isAuthenticated() const;
    bool hasRefreshToken() const;
    void setAccessToken(std::string token);
    void setTokens(std::string accessToken, std::string refreshToken);
    void setProfileName(std::string profileName);
    void clear();
    const std::string& accessToken() const;
    const std::string& refreshToken() const;
    const std::string& profileName() const;

private:
    std::string accessToken_;
    std::string refreshToken_;
    std::string profileName_;
};
