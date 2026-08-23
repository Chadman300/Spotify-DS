# Spotify-DS

A Spotify companion app for Nintendo 3DS. Control your Spotify playback and browse your library from your handheld console.

**Status:** ✓ **Build complete!** The 3DSX homebrew executable is built and ready to run.

## Features

✓ **OAuth 2.0 Authentication** - Sign in securely using PKCE flow  
✓ **Playlist Browsing** - View all your Spotify playlists  
✓ **Track Search** - Search for tracks across Spotify's catalog  
✓ **Now Playing** - View current playback info  
✓ **Playback Control** - Play/queue tracks on your connected device  
✓ **Persistent Configuration** - Auto-save auth tokens and UI state  
✓ **Multi-View UI** - 5 different screens for browsing  
✓ **Software Keyboard** - On-screen text entry for searches and auth codes  

## Building

### Requirements
- devkitPro with devkitARM and libctru installed
- arm-none-eabi-g++ compiler toolchain
- bash/msys2 environment
- 3dsxtool (included with libctru)

### Quick Build (Windows with devkitPro installed)

```bash
# Use msys2 bash from devkitPro
C:\devkitPro\msys2\usr\bin\bash.exe -c "
  export DEVKITPRO=/c/devkitPro
  export DEVKITARM=/c/devkitPro/devkitARM
  export CTRULIB=/c/devkitPro/libctru
  export PATH=/c/devkitPro/devkitARM/bin:/c/devkitPro/tools/bin:/usr/bin:/bin
  cd /c/Users/vital/source/repos/Spotify-DS
  make clean
  make
"
```

**Output:** `build/Spotify-DS.3dsx` (188 KB) - Ready to run on 3DS

## Installation & Usage

1. **Copy to 3DS:**
   - Copy `build/Spotify-DS.3dsx` to `sd:/3ds/` on your 3DS SD card
   - Launch from Homebrew Launcher

2. **First Run:**
   - Press START to open authorization screen
   - Visit the displayed Spotify authorization URL on your computer/phone
   - Enter the authorization code shown by the browser

3. **Navigation:**
   - **A** - Select/Confirm
   - **B** - Back/Cancel  
   - **X** - Refresh current view
   - **Y** - Cycle through views
   - **D-Pad** - Navigate menus
   - **L + START** - Sign out

4. **Views:**
   - **Home** - Profile and status
   - **Playlists** - Browse your playlists
   - **Playlist Detail** - View tracks (select one to queue)
   - **Search** - Search Spotify catalog
   - **Now Playing** - Current track and queue

## Architecture

**Companion App Model:** This is a remote control for Spotify running elsewhere. The 3DS doesn't play audio - it controls Spotify via Web API.

### Code Structure
```
source/
├── main.cpp              - Entry point
├── app.cpp              - UI loop and rendering
├── spotify_client.cpp   - Facade/coordinator
├── spotify_api.cpp      - HTTP API calls
├── spotify_auth.cpp     - OAuth 2.0 PKCE
├── spotify_config.cpp   - Persistent config
└── spotify_session.cpp  - Token/profile storage
```

### Authentication
1. Generate SHA-256 PKCE challenge from random verifier
2. Construct authorization URL with challenge
3. User visits URL, approves, gets code
4. Exchange code for access token
5. Token stored in `sdmc:/3ds/Spotify-DS/config.txt`

## CIA Packaging (Advanced)

To create a `.cia` for CIA installers (FBI, Anemone, etc.):

**Required files:**
- `banner.png` - 320×240 image
- `banner.wav` - 16-bit mono audio, 16000 Hz
- `build-cia.rsf` - System config (included)
- `icon.png` - 48×48 icon (default provided)

**Build CIA (requires makerom + bannertool):**
```bash
bannertool makebanner -i banner.png -a banner.wav -o banner.bin
makerom -f cia -target t -exefslogo -o Spotify-DS.cia \
  -elf build/Spotify-DS.elf -rsf build-cia.rsf \
  -banner banner.bin -icon icon.png
```

**Note:** Standard `.3dsx` homebrew format (already built) is the recommended distribution method. CIA requires additional tools and assets.

## Configuration File

Location: `sdmc:/3ds/Spotify-DS/config.txt`

```ini
client_id=YOUR_CLIENT_ID
redirect_uri=YOUR_REDIRECT_URI
scope=playlist-read-private%20playlist-read-collaborative%20user-read-private%20user-modify-playback-state%20user-read-currently-playing
last_search_query=
last_playlist_id=
last_view=0
access_token=
refresh_token=
```

Edit `client_id` with your Spotify developer app credentials before first run.

## Limitations

- **Companion only** - Requires active Spotify device elsewhere
- **No offline playback** - Requires WiFi on 3DS
- **No local cache** - Metadata fetched live via API
- **Free tier limited** - Spotify free users get reduced skips

## License

Educational/hobbyist project. Spotify is a trademark of Spotify AB.
