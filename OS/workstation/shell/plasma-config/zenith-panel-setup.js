var zenithLaunchers = [
    "applications:os.zenith.Welcome.desktop",
    "applications:os.zenith.Terminal.desktop",
    "applications:os.zenith.Settings.desktop",
    "applications:os.zenith.Files.desktop",
    "applications:os.zenith.Packages.desktop",
    "applications:os.zenith.Installer.desktop",
    "applications:zenith-build.desktop"
].join(",");
var zenithWallpaper = "file:///usr/share/backgrounds/zenith/zenith-login-wallpaper.png";

for (var i = panelIds.length - 1; i >= 0; --i) {
    var oldPanel = panelById(panelIds[i]);
    if (oldPanel) {
        oldPanel.remove();
    }
}

var panel = new Panel;
panel.location = "bottom";
panel.height = Math.max(44, 2 * Math.floor(gridUnit * 2.5 / 2));

var screen = panel.screen;
if (panel.formFactor === "horizontal") {
    var geo = screenGeometry(screen);
    var maximumWidth = Math.ceil(geo.height * 21 / 9);
    if (geo.width > maximumWidth) {
        panel.alignment = "center";
        panel.minimumLength = maximumWidth;
        panel.maximumLength = maximumWidth;
    }
}

panel.addWidget("org.kde.plasma.kickoff");

var tasks = panel.addWidget("org.kde.plasma.icontasks");
tasks.currentConfigGroup = ["General"];
tasks.writeConfig("launchers", zenithLaunchers);

panel.addWidget("org.kde.plasma.marginsseparator");
panel.addWidget("org.kde.plasma.systemtray");
panel.addWidget("org.kde.plasma.digitalclock");

var desktopsArray = desktopsForActivity(currentActivity());
for (var j = 0; j < desktopsArray.length; j++) {
    var desktop = desktopsArray[j];
    desktop.wallpaperPlugin = "org.kde.image";
    desktop.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
    desktop.writeConfig("Image", zenithWallpaper);
    desktop.writeConfig("FillMode", 2);
    desktop.currentConfigGroup = [];
}
