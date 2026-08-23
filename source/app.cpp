#include "app.hpp"
#include "qrcodegen.h"

#include <3ds.h>
#include <3ds/applets/swkbd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#include "quirc.h"

namespace {
PrintConsole gBottomConsole;
}

App::App() {
    state_.statusMessage = "Spotify-DS ready (scan build)";
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

    consoleSelect(&gBottomConsole);
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

namespace {
constexpr int kTopScreenWidth = 400;
constexpr int kTopScreenHeight = 240;
constexpr int kCameraFramePixels = kTopScreenWidth * kTopScreenHeight;
constexpr int kCameraFrameBytesRgb565 = kCameraFramePixels * 2;
constexpr u64 kCameraWaitTimeout = 50000000ULL;
constexpr int kDecodeWidth = 200;
constexpr int kDecodeHeight = 120;

size_t framebufferByteCount(gfxScreen_t screen, u16 width, u16 height) {
    const unsigned bytesPerPixel = gspGetBytesPerPixel(gfxGetScreenFormat(screen));
    return static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;
}

void clearFramebuffer(gfxScreen_t screen, gfx3dSide_t side) {
    u16 width = 0;
    u16 height = 0;
    u8* fb = gfxGetFramebuffer(screen, side, &width, &height);
    if (!fb || width == 0 || height == 0) {
        return;
    }
    std::memset(fb, 0, framebufferByteCount(screen, width, height));
}

void clearAllScreenBuffers() {
    clearFramebuffer(GFX_TOP, GFX_LEFT);
    clearFramebuffer(GFX_TOP, GFX_RIGHT);
    clearFramebuffer(GFX_BOTTOM, GFX_LEFT);
}

void putLandscapePixelBgr8(u8* framebuffer, int x, int y, bool isDark) {
    if (!framebuffer || x < 0 || x >= kTopScreenWidth || y < 0 || y >= kTopScreenHeight) {
        return;
    }

    const size_t index = (static_cast<size_t>(x) * static_cast<size_t>(kTopScreenHeight)
        + static_cast<size_t>(kTopScreenHeight - 1 - y)) * 3u;
    const u8 value = isDark ? 0 : 255;
    framebuffer[index + 0] = value;
    framebuffer[index + 1] = value;
    framebuffer[index + 2] = value;
}

void putLandscapePixelColorBgr8(u8* framebuffer, int x, int y, u8 r, u8 g, u8 b) {
    if (!framebuffer || x < 0 || x >= kTopScreenWidth || y < 0 || y >= kTopScreenHeight) {
        return;
    }

    const size_t index = (static_cast<size_t>(x) * static_cast<size_t>(kTopScreenHeight)
        + static_cast<size_t>(kTopScreenHeight - 1 - y)) * 3u;
    framebuffer[index + 0] = r;
    framebuffer[index + 1] = g;
    framebuffer[index + 2] = b;
}

void fillLandscapeRectBgr8(u8* framebuffer, int x, int y, int width, int height, bool isDark) {
    for (int yy = y; yy < y + height; ++yy) {
        for (int xx = x; xx < x + width; ++xx) {
            putLandscapePixelBgr8(framebuffer, xx, yy, isDark);
        }
    }
}

[[maybe_unused]] bool drawAuthQrOnTopScreen(const std::string& authUrl) {
    if (authUrl.empty()) {
        return false;
    }

    std::array<uint8_t, qrcodegen_BUFFER_LEN_MAX> qrTemp{};
    std::array<uint8_t, qrcodegen_BUFFER_LEN_MAX> qrCode{};
    const bool encoded = qrcodegen_encodeText(
        authUrl.c_str(),
        qrTemp.data(),
        qrCode.data(),
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true);
    if (!encoded) {
        return false;
    }

    const int qrSize = qrcodegen_getSize(qrCode.data());
    const int quiet = 2;
    const int qrRegionSize = 220;
    const int qrRegionX = (kTopScreenWidth - qrRegionSize) / 2;
    const int qrRegionY = (kTopScreenHeight - qrRegionSize) / 2;
    const int modulesWithQuiet = qrSize + quiet * 2;
    const int scale = std::max(1, qrRegionSize / modulesWithQuiet);
    const int drawSize = modulesWithQuiet * scale;
    const int originX = qrRegionX + (qrRegionSize - drawSize) / 2;
    const int originY = qrRegionY + (qrRegionSize - drawSize) / 2;

    u8* topLeft = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
    u8* topRight = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, nullptr, nullptr);
    if (!topLeft || !topRight) {
        return false;
    }

    fillLandscapeRectBgr8(topLeft, 0, 0, kTopScreenWidth, kTopScreenHeight, false);
    fillLandscapeRectBgr8(topRight, 0, 0, kTopScreenWidth, kTopScreenHeight, false);

    for (int y = 0; y < qrSize; ++y) {
        for (int x = 0; x < qrSize; ++x) {
            if (!qrcodegen_getModule(qrCode.data(), x, y)) {
                continue;
            }

            const int px = originX + (x + quiet) * scale;
            const int py = originY + (y + quiet) * scale;
            fillLandscapeRectBgr8(topLeft, px, py, scale, scale, true);
            fillLandscapeRectBgr8(topRight, px, py, scale, scale, true);
        }
    }

    return true;
}

[[maybe_unused]] bool captureCameraFrameRgb565(std::vector<u16>* outFrame) {
    if (!outFrame) {
        return false;
    }

    outFrame->assign(static_cast<size_t>(kCameraFramePixels), 0);

    Result rc = camInit();
    if (R_FAILED(rc)) {
        return false;
    }

    bool success = true;
    Handle camReceiveEvent = 0;

    rc = CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    success = success && R_SUCCEEDED(rc);
    rc = CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    success = success && R_SUCCEEDED(rc);
    rc = CAMU_SetNoiseFilter(SELECT_OUT1, true);
    success = success && R_SUCCEEDED(rc);
    rc = CAMU_SetAutoExposure(SELECT_OUT1, true);
    success = success && R_SUCCEEDED(rc);
    rc = CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    success = success && R_SUCCEEDED(rc);

    u32 transferBytes = 0;
    rc = CAMU_GetMaxBytes(&transferBytes, kTopScreenWidth, kTopScreenHeight);
    success = success && R_SUCCEEDED(rc);
    if (success) {
        rc = CAMU_SetTransferBytes(PORT_CAM1, transferBytes, kTopScreenWidth, kTopScreenHeight);
        success = success && R_SUCCEEDED(rc);
    }

    if (success) {
        rc = CAMU_Activate(SELECT_OUT1);
        success = success && R_SUCCEEDED(rc);
    }
    if (success) {
        rc = CAMU_ClearBuffer(PORT_CAM1);
        success = success && R_SUCCEEDED(rc);
    }
    if (success) {
        rc = CAMU_StartCapture(PORT_CAM1);
        success = success && R_SUCCEEDED(rc);
    }
    if (success) {
        rc = CAMU_SetReceiving(&camReceiveEvent,
                               reinterpret_cast<u8*>(outFrame->data()),
                               PORT_CAM1,
                               kCameraFrameBytesRgb565,
                               static_cast<s16>(transferBytes));
        success = success && R_SUCCEEDED(rc);
    }
    if (success && camReceiveEvent) {
        rc = svcWaitSynchronization(camReceiveEvent, kCameraWaitTimeout);
        success = success && R_SUCCEEDED(rc);
    }

    CAMU_StopCapture(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    if (camReceiveEvent) {
        svcCloseHandle(camReceiveEvent);
    }
    camExit();

    return success;
}

bool decodeQrPayloadFromRgb565(const std::vector<u16>& frame, std::string* outPayload) {
    if (!outPayload || frame.size() != static_cast<size_t>(kCameraFramePixels)) {
        return false;
    }

    static quirc* qr = nullptr;
    static bool qrReady = false;
    if (!qr) {
        qr = quirc_new();
        if (!qr) {
            return false;
        }
    }
    if (!qrReady) {
        qrReady = quirc_resize(qr, kDecodeWidth, kDecodeHeight) == 0;
        if (!qrReady) {
            return false;
        }
    }

    bool ok = false;
    int width = 0;
    int height = 0;
    uint8_t* image = quirc_begin(qr, &width, &height);
    if (image && width == kDecodeWidth && height == kDecodeHeight) {
        for (int y = 0; y < kDecodeHeight; ++y) {
            const int srcY = y * 2;
            for (int x = 0; x < kDecodeWidth; ++x) {
                const int srcX = x * 2;
                const u16 pixel = frame[static_cast<size_t>(srcY) * static_cast<size_t>(kTopScreenWidth) + static_cast<size_t>(srcX)];
                const int r = ((pixel >> 11) & 0x1F) << 3;
                const int g = ((pixel >> 5) & 0x3F) << 2;
                const int b = (pixel & 0x1F) << 3;
                image[static_cast<size_t>(y) * static_cast<size_t>(kDecodeWidth) + static_cast<size_t>(x)] = static_cast<uint8_t>((r * 30 + g * 59 + b * 11) / 100);
            }
        }
        quirc_end(qr);

        const int count = quirc_count(qr);
        for (int i = 0; i < count; ++i) {
            struct quirc_code code;
            struct quirc_data data;
            quirc_extract(qr, i, &code);

            quirc_decode_error_t err = quirc_decode(&code, &data);
            if (err != QUIRC_SUCCESS) {
                quirc_flip(&code);
                err = quirc_decode(&code, &data);
            }
            if (err == QUIRC_SUCCESS && data.payload_len > 0) {
                *outPayload = std::string(reinterpret_cast<const char*>(data.payload), static_cast<size_t>(data.payload_len));
                ok = true;
                break;
            }
        }
    }
    return ok;
}

void drawCameraPreviewToTop(const std::vector<u16>& frame) {
    if (frame.size() != static_cast<size_t>(kCameraFramePixels)) {
        return;
    }

    u8* topLeft = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
    if (!topLeft) {
        return;
    }

    for (int y = 0; y < kTopScreenHeight; ++y) {
        for (int x = 0; x < kTopScreenWidth; ++x) {
            const u16 pixel = frame[static_cast<size_t>(y) * static_cast<size_t>(kTopScreenWidth) + static_cast<size_t>(x)];
            const u8 r = static_cast<u8>(((pixel >> 11) & 0x1F) << 3);
            const u8 g = static_cast<u8>(((pixel >> 5) & 0x3F) << 2);
            const u8 b = static_cast<u8>((pixel & 0x1F) << 3);
            putLandscapePixelColorBgr8(topLeft, x, y, r, g, b);
        }
    }
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1;
}

std::string urlDecode(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '%' && i + 2 < value.size()) {
            const int hi = hexValue(value[i + 1]);
            const int lo = hexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                output.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (ch == '+') {
            output.push_back(' ');
        } else {
            output.push_back(ch);
        }
    }
    return output;
}

std::string extractAuthorizationCode(const std::string& input) {
    const size_t codePos = input.find("code=");
    if (codePos == std::string::npos) {
        return input;
    }

    size_t valueStart = codePos + 5;
    size_t valueEnd = input.find('&', valueStart);
    if (valueEnd == std::string::npos) {
        valueEnd = input.find('#', valueStart);
    }
    if (valueEnd == std::string::npos) {
        valueEnd = input.size();
    }

    return urlDecode(std::string_view(input).substr(valueStart, valueEnd - valueStart));
}
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
    state_.statusMessage = "A starts live scan. X opens manual paste.";
}

void App::scanSignInCode() {
    consoleSelect(&gBottomConsole);
    std::vector<u16> cameraFrame(static_cast<size_t>(kCameraFramePixels), 0);
    state_.statusMessage = "Scanning... point camera at website QR. B cancel";

    consoleClear();
    printf("Scanning for QR code...\n\n");
    printf("Point the 3DS camera at the QR shown on the\n");
    printf("website callback page. Press B to cancel.\n");

    Result rc = camInit();
    if (R_FAILED(rc)) {
        state_.statusMessage = "Camera init failed. X paste or B cancel";
        return;
    }

    bool configured = true;
    configured = configured && R_SUCCEEDED(CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A));
    configured = configured && R_SUCCEEDED(CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A));
    configured = configured && R_SUCCEEDED(CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_30));
    configured = configured && R_SUCCEEDED(CAMU_SetNoiseFilter(SELECT_OUT1, true));
    configured = configured && R_SUCCEEDED(CAMU_SetAutoExposure(SELECT_OUT1, true));
    configured = configured && R_SUCCEEDED(CAMU_SetAutoWhiteBalance(SELECT_OUT1, true));
    configured = configured && R_SUCCEEDED(CAMU_SetTrimming(PORT_CAM1, false));

    u32 transferBytes = 0;
    configured = configured && R_SUCCEEDED(CAMU_GetMaxBytes(&transferBytes, kTopScreenWidth, kTopScreenHeight));
    if (configured) {
        configured = configured && R_SUCCEEDED(CAMU_SetTransferBytes(PORT_CAM1, transferBytes, kTopScreenWidth, kTopScreenHeight));
    }
    if (configured) {
        configured = configured && R_SUCCEEDED(CAMU_Activate(SELECT_OUT1));
    }

    // Arm the receive buffer/event BEFORE starting capture so the first frame is
    // delivered reliably; arming after StartCapture leaves the event unsignaled.
    Handle camReceiveEvent = 0;
    if (configured) {
        configured = configured && R_SUCCEEDED(CAMU_ClearBuffer(PORT_CAM1));
    }
    if (configured) {
        configured = configured && R_SUCCEEDED(CAMU_SetReceiving(&camReceiveEvent,
                                   reinterpret_cast<u8*>(cameraFrame.data()),
                                   PORT_CAM1,
                                   kCameraFrameBytesRgb565,
                                   static_cast<s16>(transferBytes)));
    }
    if (configured) {
        configured = configured && R_SUCCEEDED(CAMU_StartCapture(PORT_CAM1));
    }

    bool cancelled = false;
    bool decoded = false;
    std::string code;
    int framesSinceDecode = 0;

    if (configured) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_B) {
                cancelled = true;
                break;
            }

            rc = svcWaitSynchronization(camReceiveEvent, kCameraWaitTimeout);
            if (R_FAILED(rc)) {
                // Frame not ready yet; loop so B stays responsive.
                continue;
            }
            svcCloseHandle(camReceiveEvent);
            camReceiveEvent = 0;

            drawCameraPreviewToTop(cameraFrame);
            gfxFlushBuffers();
            gfxScreenSwapBuffers(GFX_TOP, false);
            gspWaitForVBlank();

            // Decoding is heavy; run it only every few frames so the live
            // preview stays smooth instead of stalling on every frame.
            if (++framesSinceDecode >= 4) {
                framesSinceDecode = 0;
                std::string payload;
                if (decodeQrPayloadFromRgb565(cameraFrame, &payload)) {
                    code = extractAuthorizationCode(payload);
                    decoded = !code.empty();
                    if (decoded) {
                        break;
                    }
                }
            }

            if (R_FAILED(CAMU_SetReceiving(&camReceiveEvent,
                                           reinterpret_cast<u8*>(cameraFrame.data()),
                                           PORT_CAM1,
                                           kCameraFrameBytesRgb565,
                                           static_cast<s16>(transferBytes)))) {
                break;
            }
        }
    }

    CAMU_StopCapture(PORT_CAM1);
    CAMU_Activate(SELECT_NONE);
    if (camReceiveEvent) {
        svcCloseHandle(camReceiveEvent);
    }
    camExit();

    if (cancelled) {
        state_.statusMessage = "Scan cancelled. A scan, X paste";
        return;
    }
    if (!configured || !decoded) {
        state_.statusMessage = "Scan failed. A retry, X paste";
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

void App::pasteSignInCode() {
    std::string code;
    if (!promptText("Manual Paste Login (X)", "Paste callback URL or auth code", &code, 255)) {
        state_.statusMessage = "Paste cancelled. A scan, X paste";
        return;
    }

    code = extractAuthorizationCode(code);
    if (code.empty()) {
        state_.statusMessage = "No code found. A scan, X paste";
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
    gfxSet3D(false);

    // Single-buffer both screens: the console caches its framebuffer pointer, so
    // swapping buffers under it causes text ghosting/artifacts.
    gfxSetDoubleBuffering(GFX_TOP, false);
    gfxSetDoubleBuffering(GFX_BOTTOM, false);

    // Console lives on the bottom screen so the top screen can be used purely
    // for graphics (QR code + camera preview) without fighting the console.
    consoleInit(GFX_BOTTOM, &gBottomConsole);
    consoleSelect(&gBottomConsole);
    clearAllScreenBuffers();
    consoleClear();
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
            if (state_.view == AppView::SignIn) {
                scanSignInCode();
            } else if (state_.view == AppView::Search) {
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
            if (state_.view == AppView::Search || state_.view == AppView::Playlists || state_.view == AppView::PlaylistDetail || state_.view == AppView::NowPlaying || state_.view == AppView::SignIn) {
                if (state_.view == AppView::SignIn) {
                    state_.statusMessage = "Sign-in cancelled";
                }
                refreshHome();
            } else {
                signOut();
            }
        }

        if (keys & KEY_X) {
            if (state_.view == AppView::SignIn) {
                pasteSignInCode();
            } else if (state_.view == AppView::Search && selectedTrack()) {
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

        // Only present the top (graphics) screen. The bottom console screen is
        // single-buffered and must NOT be swapped or its text ghosts/artifacts.
        gfxFlushBuffers();
        gfxScreenSwapBuffers(GFX_TOP, false);
        gspWaitForVBlank();
    }

    httpcExit();
    gfxExit();
    return 0;
}

void App::render() const {
    consoleSelect(&gBottomConsole);
    consoleClear();

    // Top screen is graphics-only: show the auth QR when the user needs to sign
    // in, otherwise keep it blank so no stale pixels remain.
    const bool wantQr = (state_.view == AppView::SignIn)
        || (state_.view == AppView::Home && !state_.isSignedIn && !state_.authUrl.empty());
    if (!wantQr || !drawAuthQrOnTopScreen(state_.authUrl)) {
        clearFramebuffer(GFX_TOP, GFX_LEFT);
        clearFramebuffer(GFX_TOP, GFX_RIGHT);
    }

    printf("%s\n\n", state_.statusMessage.c_str());
    printf("Spotify-DS companion\n");
    printf("Account: %s\n", state_.isSignedIn ? state_.activeProfileName.c_str() : "signed out");
    printf("View: %d\n\n", static_cast<int>(state_.view));

    if (state_.view == AppView::SignIn) {
        printf("Scan the QR on the TOP screen with your phone\n");
        printf("to open the Spotify login page.\n\n");
        printf("A = scan website QR with 3DS camera\n");
        printf("X = paste callback URL/code\n");
        printf("B = cancel\n");
        return;
    }

    if (!state_.isSignedIn && !state_.authUrl.empty() && state_.view == AppView::Home) {
        printf("Scan the QR on the TOP screen with your phone to sign in.\n\n");
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
            printf("\nOpen the auth URL shown above, then use A to scan or X to paste.\n");
        } else {
            printf("\nA = open playlist, Y = refresh playlists\n");
        }
    }
}
