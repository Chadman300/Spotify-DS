#include "app.hpp"

#include <3ds.h>
#include <3ds/applets/swkbd.h>

#include <cstdio>

App::App() {
    state_.statusMessage = "Spotify-DS ready";
    state_.authUrl = client_.authorizationUrl();
    state_.searchQuery = client_.config().lastSearchQuery;
    refreshHome();
}

bool App::promptText(const std::string& title, const std::string& hint, std::string* outValue, int maxLength, const std::string& initialValue) {
    if (!outValue) {
        return false;
    }

    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, maxLength);
    swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true);
    swkbdSetHintText(&swkbd, hint.c_str());
    swkbdSetInitialText(&swkbd, initialValue.c_str());

    char buffer[256] = {0};
    consoleClear();
    printf("%s\n\n", title.c_str());
    printf("%s\n\n", hint.c_str());
    SwkbdButton button = swkbdInputText(&swkbd, buffer, sizeof(buffer));
    if (button != SWKBD_BUTTON_RIGHT || buffer[0] == '\0') {
        return false;
    }

    *outValue = buffer;
    return true;
}

std::string App::friendlyError(const std::string& error) const {
    if (error.find("HTTP context") != std::string::npos || error.find("begin HTTP request") != std::string::npos || error.find("download HTTP body") != std::string::npos) {
        return "Network error. Check Wi-Fi and try again.";
    }
    if (error.find("token") != std::string::npos || error.find("profile") != std::string::npos) {
        return "Spotify auth failed or expired. Sign in again.";
    }
    if (error.empty()) {
        return "Operation failed.";
    }
    return error;
}

void App::refreshHome() {
    state_.view = AppView::Home;
    state_.selectedIndex = 0;
    state_.isSignedIn = client_.isSignedIn();
    state_.activeProfileName = client_.profileName();

    if (!state_.isSignedIn) {
        state_.playlistResults.clear();
        state_.searchResults.clear();
        state_.playlistTracks.clear();
        state_.hasNowPlaying = false;
        state_.authUrl = client_.authorizationUrl();
        state_.statusMessage = "Press A to sign in";
        client_.config().lastView = LastView::Home;
        client_.saveConfig();
        return;
    }

    refreshPlaylists();
    state_.view = AppView::Home;
    client_.config().lastView = LastView::Home;
    client_.saveConfig();
}

void App::refreshPlaylists() {
    state_.view = AppView::Playlists;
    state_.selectedIndex = 0;
    state_.isSignedIn = client_.isSignedIn();
    state_.activeProfileName = client_.profileName();

    std::string error;
    state_.playlistResults = client_.loadPlaylists(&error);
    if (!error.empty()) {
        state_.playlistResults.clear();
        state_.statusMessage = friendlyError(error);
    } else if (state_.playlistResults.empty()) {
        state_.statusMessage = "No playlists found";
    } else {
        state_.statusMessage = "Playlists loaded";
    }
    client_.config().lastView = LastView::Playlists;
    client_.saveConfig();
}

void App::refreshPlaylistDetail(const PlaylistItem& playlist) {
    state_.selectedPlaylist = playlist;
    state_.view = AppView::PlaylistDetail;
    state_.selectedIndex = 0;
    state_.playlistTracks.clear();

    std::string error;
    state_.playlistTracks = client_.loadPlaylistTracks(playlist.id, &error);
    if (!error.empty()) {
        state_.playlistTracks.clear();
        state_.statusMessage = friendlyError(error);
    } else if (state_.playlistTracks.empty()) {
        state_.statusMessage = "Playlist has no tracks";
    } else {
        state_.statusMessage = "Playlist loaded";
    }
    client_.config().lastPlaylistId = playlist.id;
    client_.config().lastView = LastView::PlaylistDetail;
    client_.saveConfig();
}

void App::refreshNowPlaying() {
    state_.view = AppView::NowPlaying;
    state_.selectedIndex = 0;
    state_.hasNowPlaying = false;
    state_.isPlaying = false;
    state_.nowPlaying = {};

    std::string error;
    state_.hasNowPlaying = client_.loadNowPlaying(&state_.nowPlaying, &state_.isPlaying, &error);
    if (!error.empty()) {
        state_.hasNowPlaying = false;
        state_.statusMessage = friendlyError(error);
    } else if (!state_.hasNowPlaying) {
        state_.statusMessage = "Nothing is playing";
    } else {
        state_.statusMessage = state_.isPlaying ? "Now playing" : "Playback paused";
    }
    client_.config().lastView = LastView::NowPlaying;
    client_.saveConfig();
}

void App::runSearch() {
    std::string query;
    if (!promptText("Spotify Search", "Type a track, artist, or album", &query, 96, client_.config().lastSearchQuery)) {
        return;
    }

    state_.searchQuery = query;
    client_.config().lastSearchQuery = query;
    state_.view = AppView::Search;
    state_.selectedIndex = 0;

    std::string error;
    state_.searchResults = client_.searchTracks(query, &error);
    if (!error.empty()) {
        state_.statusMessage = friendlyError(error);
    } else if (state_.searchResults.empty()) {
        state_.statusMessage = "No results";
    } else {
        state_.statusMessage = "Search results loaded";
    }
    client_.config().lastView = LastView::Search;
    client_.saveConfig();
}

void App::signIn() {
    state_.view = AppView::SignIn;
    state_.authUrl = client_.authorizationUrl();
    state_.statusMessage = "Open the URL and paste the callback code";
    render();

    std::string code;
    if (!promptText("Spotify Login", "Paste the code from the redirect URL", &code, 160)) {
        state_.statusMessage = "Sign-in cancelled";
        refreshHome();
        return;
    }

    std::string error;
    if (!client_.signIn(code, &error)) {
        state_.statusMessage = error.empty() ? "Sign-in failed" : friendlyError(error);
        state_.isSignedIn = false;
        return;
    }

    state_.isSignedIn = true;
    state_.activeProfileName = client_.profileName();
    state_.statusMessage = "Signed in";
    refreshHome();
    client_.config().lastView = LastView::Home;
    client_.saveConfig();
}

void App::moveSelection(int delta) {
    const int itemCount = state_.view == AppView::Search ? static_cast<int>(state_.searchResults.size())
        : state_.view == AppView::PlaylistDetail ? static_cast<int>(state_.playlistTracks.size())
        : static_cast<int>(state_.playlistResults.size());
    if (itemCount <= 0) {
        state_.selectedIndex = 0;
        return;
    }

    int nextIndex = state_.selectedIndex + delta;
    if (nextIndex < 0) {
        nextIndex = 0;
    }
    if (nextIndex >= itemCount) {
        nextIndex = itemCount - 1;
    }
    state_.selectedIndex = nextIndex;
}

const TrackItem* App::selectedTrack() const {
    if ((state_.view != AppView::Search && state_.view != AppView::PlaylistDetail) || state_.selectedIndex < 0) {
        return nullptr;
    }
    if (state_.view == AppView::Search && state_.selectedIndex < static_cast<int>(state_.searchResults.size())) {
        return &state_.searchResults[static_cast<size_t>(state_.selectedIndex)];
    }
    if (state_.view == AppView::PlaylistDetail && state_.selectedIndex < static_cast<int>(state_.playlistTracks.size())) {
        return &state_.playlistTracks[static_cast<size_t>(state_.selectedIndex)];
    }
    return nullptr;
}

const PlaylistItem* App::selectedPlaylist() const {
    if (state_.view != AppView::Playlists || state_.selectedIndex < 0 || state_.selectedIndex >= static_cast<int>(state_.playlistResults.size())) {
        return nullptr;
    }
    return &state_.playlistResults[static_cast<size_t>(state_.selectedIndex)];
}

void App::signOut() {
    client_.signOut();
    state_.isSignedIn = false;
    state_.activeProfileName.clear();
    state_.playlistResults.clear();
    state_.searchResults.clear();
    state_.playlistTracks.clear();
    state_.hasNowPlaying = false;
    state_.statusMessage = "Signed out";
    refreshHome();
    client_.config().lastView = LastView::Home;
    client_.saveConfig();
}

int App::run() {
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    httpcInit(0);

    while (aptMainLoop()) {
        hidScanInput();
        const u32 keys = hidKeysDown();

        if (keys & KEY_START) {
            break;
        }

        if (keys & KEY_UP) {
            moveSelection(-1);
        }

        if (keys & KEY_DOWN) {
            moveSelection(1);
        }

        if (keys & KEY_A) {
            if (state_.view == AppView::Search) {
                const TrackItem* track = selectedTrack();
                if (track) {
                    std::string error;
                    if (client_.playTrack(*track, &error)) {
                        state_.statusMessage = "Playing " + track->name;
                        refreshNowPlaying();
                    } else {
                        state_.statusMessage = friendlyError(error);
                    }
                }
            } else if (state_.view == AppView::PlaylistDetail) {
                const TrackItem* track = selectedTrack();
                if (track) {
                    std::string error;
                    if (client_.playTrack(*track, &error)) {
                        state_.statusMessage = "Playing " + track->name;
                        refreshNowPlaying();
                    } else {
                        state_.statusMessage = friendlyError(error);
                    }
                }
            } else if (state_.view == AppView::Playlists) {
                const PlaylistItem* playlist = selectedPlaylist();
                if (playlist) {
                    refreshPlaylistDetail(*playlist);
                }
            } else if (!state_.isSignedIn) {
                signIn();
            } else {
                refreshHome();
            }
        }

        if (keys & KEY_B) {
            if (state_.view == AppView::Search || state_.view == AppView::Playlists || state_.view == AppView::PlaylistDetail || state_.view == AppView::NowPlaying) {
                refreshHome();
            } else {
                signOut();
            }
        }

        if (keys & KEY_X) {
            if (state_.view == AppView::Search && selectedTrack()) {
                const TrackItem* track = selectedTrack();
                std::string error;
                if (track && client_.queueTrack(*track, &error)) {
                    state_.statusMessage = "Queued " + track->name;
                } else {
                    state_.statusMessage = friendlyError(error);
                }
            } else if (state_.view == AppView::PlaylistDetail && selectedTrack()) {
                const TrackItem* track = selectedTrack();
                std::string error;
                if (track && client_.queueTrack(*track, &error)) {
                    state_.statusMessage = "Queued " + track->name;
                } else {
                    state_.statusMessage = friendlyError(error);
                }
            } else {
                runSearch();
            }
        }

        if (keys & KEY_Y) {
            refreshPlaylists();
        }

        if (keys & KEY_L) {
            refreshNowPlaying();
        }

        render();

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    httpcExit();
    gfxExit();
    return 0;
}

void App::render() const {
    consoleClear();

    printf("%s\n\n", state_.statusMessage.c_str());
    printf("Spotify-DS companion\n");
    printf("Account: %s\n", state_.isSignedIn ? state_.activeProfileName.c_str() : "signed out");
    printf("View: %d\n\n", static_cast<int>(state_.view));

    if (state_.view == AppView::SignIn) {
        printf("Auth URL:\n%s\n\n", state_.authUrl.c_str());
    }

    if (!state_.isSignedIn && !state_.authUrl.empty() && state_.view == AppView::Home) {
        printf("Auth URL:\n%s\n\n", state_.authUrl.c_str());
    }

    printf("Controls:\n");
    printf("A = sign in / play selected / select playlist\n");
    printf("B = back / sign out\n");
    printf("X = search / queue selected\n");
    printf("Y = playlists\n");
    printf("L = now playing\n");
    printf("UP/DOWN = move selection\n");
    printf("START = exit\n\n");

    if (state_.view == AppView::NowPlaying) {
        if (state_.hasNowPlaying) {
            printf("Now playing:\n");
            printf("%s\n", state_.nowPlaying.name.c_str());
            printf("%s\n", state_.nowPlaying.artist.c_str());
            printf("Status: %s\n", state_.isPlaying ? "playing" : "paused");
        } else {
            printf("Nothing is playing.\n");
        }
        return;
    }

    if (state_.view == AppView::Search) {
        printf("Search: %s\n\n", state_.searchQuery.c_str());
        for (size_t index = 0; index < state_.searchResults.size(); ++index) {
            const auto& track = state_.searchResults[index];
            printf("%c %s / %s\n", index == static_cast<size_t>(state_.selectedIndex) ? '>' : ' ', track.name.c_str(), track.artist.c_str());
        }
        if (state_.searchResults.empty()) {
            printf("(no search results)\n");
        }
        printf("\nA = play, X = queue\n");
        return;
    }

    if (state_.view == AppView::PlaylistDetail) {
        printf("Playlist: %s\n", state_.selectedPlaylist.name.c_str());
        if (!state_.selectedPlaylist.description.empty()) {
            printf("%s\n", state_.selectedPlaylist.description.c_str());
        }
        printf("\n");
        for (size_t index = 0; index < state_.playlistTracks.size(); ++index) {
            const auto& track = state_.playlistTracks[index];
            printf("%c %s / %s\n", index == static_cast<size_t>(state_.selectedIndex) ? '>' : ' ', track.name.c_str(), track.artist.c_str());
        }
        if (state_.playlistTracks.empty()) {
            printf("(no tracks loaded)\n");
        }
        printf("\nA = play, X = queue, B = back\n");
        return;
    }

    if (state_.view == AppView::Playlists || state_.view == AppView::Home) {
        printf("Playlists:\n");
        for (size_t index = 0; index < state_.playlistResults.size(); ++index) {
            const auto& playlist = state_.playlistResults[index];
            printf("%c %s\n", index == static_cast<size_t>(state_.selectedIndex) ? '>' : ' ', playlist.name.c_str());
            if (!playlist.description.empty()) {
                printf("  %s\n", playlist.description.c_str());
            }
        }
        if (state_.playlistResults.empty()) {
            printf("(no playlists loaded)\n");
        }
        if (state_.view == AppView::Home && !state_.isSignedIn) {
            printf("\nOpen the auth URL shown above, then paste the code with A.\n");
        } else {
            printf("\nA = open playlist, Y = refresh playlists\n");
        }
    }
}
