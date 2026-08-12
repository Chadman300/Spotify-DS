#include "spotify_api.hpp"

#include <3ds.h>

#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>

namespace {
std::string makeTrackUri(const std::string& trackId) {
    return "spotify:track:" + trackId;
}

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}
}

SpotifyApi::SpotifyApi(SpotifySession& session)
    : session_(session) {
}

SpotifyApi::HttpResponse SpotifyApi::request(const std::string& method, const std::string& url, const std::vector<std::string>& headers, const std::string& body, const std::string& contentType, std::string* error) const {
    httpcContext context;
    HTTPC_RequestMethod requestMethod = HTTPC_METHOD_GET;
    if (method == "POST") {
        requestMethod = HTTPC_METHOD_POST;
    } else if (method == "PUT") {
        requestMethod = HTTPC_METHOD_PUT;
    } else if (method == "DELETE") {
        requestMethod = HTTPC_METHOD_DELETE;
    } else if (method == "HEAD") {
        requestMethod = HTTPC_METHOD_HEAD;
    }

    Result result = httpcOpenContext(&context, requestMethod, url.c_str(), 0);
    if (R_FAILED(result)) {
        if (error) {
            *error = "Failed to open HTTP context";
        }
        return {};
    }

    httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
    for (const auto& header : headers) {
        const size_t colon = header.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string name = header.substr(0, colon);
        std::string value = header.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') {
            value.erase(value.begin());
        }
        httpcAddRequestHeaderField(&context, name.c_str(), value.c_str());
    }

    if (!body.empty()) {
        if (!contentType.empty()) {
            httpcAddRequestHeaderField(&context, "Content-Type", contentType.c_str());
        }
        httpcAddPostDataRaw(&context, reinterpret_cast<const u32*>(body.data()), static_cast<u32>(body.size()));
    }

    result = httpcBeginRequest(&context);
    if (R_FAILED(result)) {
        httpcCloseContext(&context);
        if (error) {
            *error = "Failed to begin HTTP request";
        }
        return {};
    }

    u32 statusCode = 0;
    result = httpcGetResponseStatusCode(&context, &statusCode);
    if (R_FAILED(result)) {
        httpcCloseContext(&context);
        if (error) {
            *error = "Failed to read HTTP response status";
        }
        return {};
    }

    u32 downloadedSize = 0;
    u32 contentSize = 0;
    httpcGetDownloadSizeState(&context, &downloadedSize, &contentSize);

    HttpResponse response;
    response.statusCode = static_cast<int>(statusCode);
    if (contentSize > 0) {
        response.body.resize(contentSize);
        u32 actualDownloaded = 0;
        result = httpcDownloadData(&context, reinterpret_cast<u8*>(&response.body[0]), contentSize, &actualDownloaded);
        if (R_FAILED(result)) {
            response.body.clear();
            if (error) {
                *error = "Failed to download HTTP body";
            }
        } else {
            response.body.resize(actualDownloaded);
        }
    }

    httpcCloseContext(&context);
    return response;
}

SpotifyApi::HttpResponse SpotifyApi::requestWithAuthRetry(const std::string& method, const std::string& url, const std::vector<std::string>& headers, const std::string& body, const std::string& contentType, std::string* error) {
    HttpResponse response = request(method, url, headers, body, contentType, error);
    if (response.statusCode != 401 || !session_.hasRefreshToken()) {
        return response;
    }

    SpotifyAuthRequest refreshRequest = SpotifyAuth::createRequest();
    if (!refreshAccessToken(refreshRequest, error)) {
        return response;
    }

    std::vector<std::string> retryHeaders = headers;
    for (auto& header : retryHeaders) {
        if (startsWith(header, "Authorization:")) {
            header = "Authorization: Bearer " + session_.accessToken();
        }
    }
    return request(method, url, retryHeaders, body, contentType, error);
}

std::string SpotifyApi::urlEncode(const std::string& input) {
    std::string output;
    static constexpr char hex[] = "0123456789ABCDEF";

    for (unsigned char ch : input) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            output.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            output += "%20";
        } else {
            output.push_back('%');
            output.push_back(hex[(ch >> 4) & 0xF]);
            output.push_back(hex[ch & 0xF]);
        }
    }

    return output;
}

std::string SpotifyApi::extractJsonStringFrom(const std::string& json, size_t start) {
    size_t end = start;
    while (end < json.size()) {
        if (json[end] == '"' && (end == start || json[end - 1] != '\\')) {
            break;
        }
        ++end;
    }
    return json.substr(start, end - start);
}

std::string SpotifyApi::extractJsonString(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }

    size_t valuePos = json.find('"', keyPos + needle.size());
    if (valuePos == std::string::npos) {
        return {};
    }
    return extractJsonStringFrom(json, valuePos + 1);
}

bool SpotifyApi::extractJsonBool(const std::string& json, const std::string& key, bool defaultValue) {
    const std::string needle = "\"" + key + "\":";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return defaultValue;
    }

    size_t valuePos = keyPos + needle.size();
    if (json.compare(valuePos, 4, "true") == 0) {
        return true;
    }
    if (json.compare(valuePos, 5, "false") == 0) {
        return false;
    }
    return defaultValue;
}

std::vector<TrackItem> SpotifyApi::parseTracks(const std::string& json) {
    std::vector<TrackItem> results;
    size_t itemsPos = json.find("\"items\":[");
    if (itemsPos == std::string::npos) {
        return results;
    }

    size_t cursor = itemsPos;
    while (results.size() < 10) {
        size_t idKey = json.find("\"id\":\"", cursor);
        if (idKey == std::string::npos) {
            break;
        }
        size_t idStart = idKey + 6;
        std::string id = extractJsonStringFrom(json, idStart);

        size_t nameKey = json.find("\"name\":\"", idStart);
        if (nameKey == std::string::npos) {
            break;
        }
        std::string name = extractJsonStringFrom(json, nameKey + 8);

        std::string artist = "Unknown artist";
        size_t artistsKey = json.find("\"artists\":[", nameKey);
        if (artistsKey != std::string::npos) {
            size_t artistNameKey = json.find("\"name\":\"", artistsKey);
            if (artistNameKey != std::string::npos) {
                artist = extractJsonStringFrom(json, artistNameKey + 8);
            }
        }

        results.push_back({std::move(id), std::move(name), std::move(artist)});
        cursor = nameKey + 1;
    }

    return results;
}

std::vector<PlaylistItem> SpotifyApi::parsePlaylists(const std::string& json) {
    std::vector<PlaylistItem> results;
    size_t itemsPos = json.find("\"items\":[");
    if (itemsPos == std::string::npos) {
        return results;
    }

    size_t cursor = itemsPos;
    while (results.size() < 10) {
        size_t idKey = json.find("\"id\":\"", cursor);
        if (idKey == std::string::npos) {
            break;
        }
        size_t idStart = idKey + 6;
        std::string id = extractJsonStringFrom(json, idStart);

        size_t nameKey = json.find("\"name\":\"", idStart);
        if (nameKey == std::string::npos) {
            break;
        }
        std::string name = extractJsonStringFrom(json, nameKey + 8);

        std::string description;
        size_t descriptionKey = json.find("\"description\":\"", nameKey);
        if (descriptionKey != std::string::npos) {
            description = extractJsonStringFrom(json, descriptionKey + 16);
        }

        results.push_back({std::move(id), std::move(name), std::move(description)});
        cursor = nameKey + 1;
    }

    return results;
}

std::vector<TrackItem> SpotifyApi::parsePlaylistTracks(const std::string& json) {
    std::vector<TrackItem> results;
    size_t itemsPos = json.find("\"items\":[");
    if (itemsPos == std::string::npos) {
        return results;
    }

    size_t cursor = itemsPos;
    while (results.size() < 20) {
        size_t trackKey = json.find("\"track\":{", cursor);
        if (trackKey == std::string::npos) {
            break;
        }

        size_t idKey = json.find("\"id\":\"", trackKey);
        size_t nameKey = json.find("\"name\":\"", trackKey);
        if (idKey == std::string::npos || nameKey == std::string::npos) {
            cursor = trackKey + 8;
            continue;
        }

        std::string id = extractJsonStringFrom(json, idKey + 6);
        std::string name = extractJsonStringFrom(json, nameKey + 8);
        std::string artist = "Unknown artist";

        size_t artistsKey = json.find("\"artists\":[", trackKey);
        if (artistsKey != std::string::npos) {
            size_t artistNameKey = json.find("\"name\":\"", artistsKey);
            if (artistNameKey != std::string::npos) {
                artist = extractJsonStringFrom(json, artistNameKey + 8);
            }
        }

        results.push_back({std::move(id), std::move(name), std::move(artist)});
        cursor = nameKey + 1;
    }

    return results;
}

bool SpotifyApi::exchangeCodeForToken(const SpotifyAuthRequest& requestData, const std::string& code, std::string* error) {
    HttpResponse response = request("POST", "https://accounts.spotify.com/api/token", {}, SpotifyAuth::buildTokenExchangeBody(requestData, code), "application/x-www-form-urlencoded", error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Spotify token exchange failed";
        }
        return false;
    }

    const std::string accessToken = extractJsonString(response.body, "access_token");
    const std::string refreshToken = extractJsonString(response.body, "refresh_token");
    if (accessToken.empty()) {
        if (error) {
            *error = "Spotify response did not include an access token";
        }
        return false;
    }

    session_.setTokens(accessToken, refreshToken);
    return true;
}

bool SpotifyApi::refreshAccessToken(const SpotifyAuthRequest& requestData, std::string* error) {
    if (!session_.hasRefreshToken()) {
        if (error) {
            *error = "No refresh token available";
        }
        return false;
    }

    HttpResponse response = request("POST", "https://accounts.spotify.com/api/token", {}, SpotifyAuth::buildRefreshBody(requestData, session_.refreshToken()), "application/x-www-form-urlencoded", error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Spotify token refresh failed";
        }
        return false;
    }

    const std::string accessToken = extractJsonString(response.body, "access_token");
    const std::string refreshToken = extractJsonString(response.body, "refresh_token");
    if (accessToken.empty()) {
        if (error) {
            *error = "Spotify response did not include a refreshed access token";
        }
        return false;
    }

    session_.setTokens(accessToken, refreshToken.empty() ? session_.refreshToken() : refreshToken);
    return true;
}

bool SpotifyApi::loadProfile(std::string* error) {
    HttpResponse response = requestWithAuthRetry("GET", "https://api.spotify.com/v1/me", {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to load Spotify profile";
        }
        return false;
    }

    std::string displayName = extractJsonString(response.body, "display_name");
    if (displayName.empty()) {
        displayName = extractJsonString(response.body, "id");
    }
    session_.setProfileName(displayName);
    return true;
}

std::vector<TrackItem> SpotifyApi::searchTracks(const std::string& query, std::string* error) {
    if (query.empty()) {
        return {};
    }

    const std::string url = "https://api.spotify.com/v1/search?type=track&limit=10&q=" + urlEncode(query);
    HttpResponse response = requestWithAuthRetry("GET", url, {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Spotify search failed";
        }
        return {};
    }

    return parseTracks(response.body);
}

std::vector<PlaylistItem> SpotifyApi::loadPlaylists(std::string* error) {
    HttpResponse response = requestWithAuthRetry("GET", "https://api.spotify.com/v1/me/playlists?limit=10", {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to load Spotify playlists";
        }
        return {};
    }

    return parsePlaylists(response.body);
}

std::vector<TrackItem> SpotifyApi::loadPlaylistTracks(const std::string& playlistId, std::string* error) {
    if (playlistId.empty()) {
        return {};
    }

    const std::string url = "https://api.spotify.com/v1/playlists/" + urlEncode(playlistId) + "/tracks?limit=20";
    HttpResponse response = requestWithAuthRetry("GET", url, {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to load playlist tracks";
        }
        return {};
    }

    return parsePlaylistTracks(response.body);
}

bool SpotifyApi::loadNowPlaying(TrackItem* track, bool* isPlaying, std::string* error) {
    HttpResponse response = requestWithAuthRetry("GET", "https://api.spotify.com/v1/me/player/currently-playing", {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode == 204) {
        if (track) {
            *track = {};
        }
        if (isPlaying) {
            *isPlaying = false;
        }
        return true;
    }

    if (response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to load current playback";
        }
        return false;
    }

    if (isPlaying) {
        *isPlaying = extractJsonBool(response.body, "is_playing", false);
    }
    if (track) {
        track->id = extractJsonString(response.body, "id");
        track->name = extractJsonString(response.body, "name");
        track->artist = "Unknown artist";
        size_t artistsKey = response.body.find("\"artists\":[");
        if (artistsKey != std::string::npos) {
            size_t artistNameKey = response.body.find("\"name\":\"", artistsKey);
            if (artistNameKey != std::string::npos) {
                track->artist = extractJsonStringFrom(response.body, artistNameKey + 8);
            }
        }
    }

    return true;
}

bool SpotifyApi::playTrack(const TrackItem& track, std::string* error) {
    const std::string body = "{\"uris\":[\"" + makeTrackUri(track.id) + "\"]}";
    HttpResponse response = requestWithAuthRetry("PUT", "https://api.spotify.com/v1/me/player/play", {"Authorization: Bearer " + session_.accessToken()}, body, "application/json", error);
    if (response.statusCode != 204 && response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to start playback";
        }
        return false;
    }
    return true;
}

bool SpotifyApi::queueTrack(const TrackItem& track, std::string* error) {
    const std::string url = "https://api.spotify.com/v1/me/player/queue?uri=" + urlEncode(makeTrackUri(track.id));
    HttpResponse response = requestWithAuthRetry("POST", url, {"Authorization: Bearer " + session_.accessToken()}, "", std::string(), error);
    if (response.statusCode != 204 && response.statusCode != 200) {
        if (error && error->empty()) {
            *error = "Failed to add track to queue";
        }
        return false;
    }
    return true;
}