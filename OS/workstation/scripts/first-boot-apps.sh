#!/bin/bash
# ZenithOS First Boot - Install Popular Applications
# This script runs on first boot to install popular apps via Flatpak.

set -e

state_dir="/var/lib/zenith"
installed_flag="$state_dir/apps-installed.flag"
deferred_flag="$state_dir/apps-install-deferred.flag"
status_file="$state_dir/apps-install-status.env"

write_status() {
    local state="$1"
    local reason="${2:-}"
    mkdir -p "$state_dir"
    {
        printf 'STATE=%s\n' "$state"
        printf 'REASON=%s\n' "$reason"
        printf 'UPDATED_UTC=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    } > "$status_file"
}

defer_apps() {
    local reason="$1"
    mkdir -p "$state_dir"
    printf '%s\n' "$reason" > "$deferred_flag"
    write_status "deferred" "$reason"
    echo "Application installation deferred: $reason"
    echo "The service will try again on a later boot."
    exit 0
}

install_app() {
    local label="$1"
    local app_id="$2"

    echo "Installing $label..."
    if flatpak list --app --columns=application | grep -qx "$app_id"; then
        echo "$label already installed"
        return
    fi

    if flatpak install -y flathub "$app_id"; then
        echo "$label installed"
        return
    fi

    defer_apps "could not install $label from Flathub"
}

echo "🚀 ZenithOS First Boot Application Installer"
echo "============================================="
echo ""

# Check if we're running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

mkdir -p "$state_dir"
write_status "running" "first boot application setup is running"

# Check if Flatpak is available
if ! command -v flatpak &> /dev/null; then
    echo "Flatpak is not installed. Installing..."
    if ! apt-get update || ! apt-get install -y flatpak; then
        defer_apps "flatpak is unavailable and could not be installed"
    fi
fi

if ! getent hosts flathub.org >/dev/null 2>&1; then
    defer_apps "DNS cannot resolve flathub.org"
fi

# Add Flathub repository if not already added
if ! flatpak remotes --columns=name | grep -qx flathub; then
    echo "Adding Flathub repository..."
    if ! flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo; then
        defer_apps "Flathub is unreachable"
    fi
fi

echo ""
echo "Installing popular applications..."
echo ""

install_app "Visual Studio Code" "com.visualstudio.code"
install_app "Google Chrome" "com.google.Chrome"
install_app "Firefox" "org.mozilla.firefox"
install_app "Discord" "com.discordapp.Discord"
install_app "Spotify" "com.spotify.Client"
install_app "Telegram" "org.telegram.desktop"
install_app "OBS Studio" "com.obsproject.Studio"
install_app "VLC" "org.videolan.VLC"
install_app "GitKraken" "com.axosoft.GitKraken"
install_app "Postman" "com.getpostman.Postman"

echo ""
echo "============================================="
echo "✨ Application installation complete!"
echo ""
echo "Installed applications:"
echo "  📝 Visual Studio Code (IDE)"
echo "  🌐 Google Chrome (Browser)"
echo "  🦊 Firefox (Browser)"
echo "  💬 Discord (Communication)"
echo "  ✈️  Telegram (Messaging)"
echo "  🎵 Spotify (Music)"
echo "  🎥 OBS Studio (Streaming)"
echo "  📺 VLC (Media Player)"
echo "  🌳 GitKraken (Git GUI)"
echo "  🔧 Postman (API Dev)"
echo ""
echo "You can also install more apps with:"
echo "  flatpak search <app-name>"
echo "  flatpak install flathub <app-id>"
echo ""
echo "Or browse https://flathub.org"
echo ""

rm -f "$deferred_flag"
write_status "complete" "first boot application setup complete"
touch "$installed_flag"
echo "🎉 First boot application setup complete!"
