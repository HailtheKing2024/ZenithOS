#!/bin/bash
# ZenithOS First Boot - Install Popular Applications
# This script runs on first boot to install VS Code, Chrome, and other popular apps via Flatpak

set -e

echo "🚀 ZenithOS First Boot Application Installer"
echo "============================================="
echo ""

# Check if we're running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

# Check if Flatpak is available
if ! command -v flatpak &> /dev/null; then
    echo "❌ Flatpak is not installed. Installing..."
    apt update
    apt install -y flatpak
fi

# Add Flathub repository if not already added
if ! flatpak remotes | grep -q flathub; then
    echo "📦 Adding Flathub repository..."
    flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
fi

echo ""
echo "Installing popular applications..."
echo ""

# Install VS Code
echo "📝 Installing Visual Studio Code..."
if ! flatpak list --app | grep -q com.visualstudio.code; then
    flatpak install -y flathub com.visualstudio.code
    echo "✅ VS Code installed"
else
    echo "✅ VS Code already installed"
fi

# Install Google Chrome
echo "🌐 Installing Google Chrome..."
if ! flatpak list --app | grep -q com.google.Chrome; then
    flatpak install -y flathub com.google.Chrome
    echo "✅ Chrome installed"
else
    echo "✅ Chrome already installed"
fi

# Optional: Install Firefox (some users prefer it)
echo "🦊 Installing Firefox..."
if ! flatpak list --app | grep -q org.mozilla.firefox; then
    flatpak install -y flathub org.mozilla.firefox
    echo "✅ Firefox installed"
else
    echo "✅ Firefox already installed"
fi

# Optional: Install Discord (for communication)
echo "💬 Installing Discord..."
if ! flatpak list --app | grep -q com.discordapp.Discord; then
    flatpak install -y flathub com.discordapp.Discord
    echo "✅ Discord installed"
else
    echo "✅ Discord already installed"
fi

# Optional: Install Spotify (for music)
echo "🎵 Installing Spotify..."
if ! flatpak list --app | grep -q com.spotify.Client; then
    flatpak install -y flathub com.spotify.Client
    echo "✅ Spotify installed"
else
    echo "✅ Spotify already installed"
fi

# Optional: Install Telegram (lightweight messaging)
echo "✈️ Installing Telegram..."
if ! flatpak list --app | grep -q org.telegram.desktop; then
    flatpak install -y flathub org.telegram.desktop
    echo "✅ Telegram installed"
else
    echo "✅ Telegram already installed"
fi

# Optional: Install OBS Studio (for streaming/recording)
echo "🎥 Installing OBS Studio..."
if ! flatpak list --app | grep -q com.obsproject.Studio; then
    flatpak install -y flathub com.obsproject.Studio
    echo "✅ OBS Studio installed"
else
    echo "✅ OBS Studio already installed"
fi

# Optional: Install VLC (media player)
echo "📺 Installing VLC..."
if ! flatpak list --app | grep -q org.videolan.VLC; then
    flatpak install -y flathub org.videolan.VLC
    echo "✅ VLC installed"
else
    echo "✅ VLC already installed"
fi

# Optional: Install GitKraken (Git GUI)
echo "🌳 Installing GitKraken..."
if ! flatpak list --app | grep -q com.axosoft.GitKraken; then
    flatpak install -y flathub com.axosoft.GitKraken
    echo "✅ GitKraken installed"
else
    echo "✅ GitKraken already installed"
fi

# Optional: Install Postman (API development)
echo "🔧 Installing Postman..."
if ! flatpak list --app | grep -q com.getpostman.Postman; then
    flatpak install -y flathub com.getpostman.Postman
    echo "✅ Postman installed"
else
    echo "✅ Postman already installed"
fi

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

# Mark first boot as complete
touch /var/lib/zenith/apps-installed.flag
echo "🎉 First boot application setup complete!"