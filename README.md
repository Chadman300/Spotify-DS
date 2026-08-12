# Spotify-DS

Spotify-DS is a Nintendo 3DS homebrew companion for Spotify.

Current scope:
- browse and search Spotify metadata
- control playback on an existing Spotify device
- show now-playing information and cover art when available
- persist Spotify app settings and the last search query

Not in scope for the first implementation:
- local audio playback on the 3DS itself

## Starter status

This repository currently contains the initial project scaffold and placeholder modules for:
- app state
- Spotify session storage
- Spotify client surface
- the 3DS entry point

## Current implementation

- Uses Spotify OAuth PKCE for sign-in.
- Prompts for the authorization code on-device using the 3DS software keyboard.
- Loads playlists, search results, and now-playing metadata from the Spotify Web API.
- Sends play and queue actions to an active Spotify device.
- Stores client settings in `sdmc:/3ds/Spotify-DS/config.txt`.
- Supports playlist detail browsing with track selection.
- Remembers the last search query between launches.

## Configuration

Set your Spotify developer app client ID in `sdmc:/3ds/Spotify-DS/config.txt` before using the app on hardware.

The generated config file currently stores:
- `client_id`
- `redirect_uri`
- `scope`
- `last_search_query`
- `last_playlist_id`
- `last_view`

## Next steps

1. Add Spotify OAuth PKCE flow.
2. Add HTTPS networking and API request helpers.
3. Replace placeholder search results with live Spotify Web API calls.
4. Build browse, queue, and now-playing screens for the 3DS UI.
