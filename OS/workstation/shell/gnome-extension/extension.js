import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const ZENITH_APPS = [
    {
        name: 'Welcome',
        subtitle: 'Start here for system status and shortcuts',
        command: 'zenith-welcome',
        icon: 'help-about-symbolic'
    },
    {
        name: 'Terminal',
        subtitle: 'Shell, logs, and self-hosted build commands',
        command: 'zenith-terminal',
        icon: 'utilities-terminal-symbolic'
    },
    {
        name: 'Settings',
        subtitle: 'System controls tuned for ZenithOS',
        command: 'zenith-settings',
        icon: 'emblem-system-symbolic'
    },
    {
        name: 'Files',
        subtitle: 'Browse the live workspace and system tree',
        command: 'zenith-files',
        icon: 'folder-symbolic'
    },
    {
        name: 'Packages',
        subtitle: 'APT, Flatpak, cache, and rollback controls',
        command: 'zenith-packages',
        icon: 'system-software-install-symbolic'
    },
    {
        name: 'Installer',
        subtitle: 'Preview install readiness without disk writes',
        command: 'zenith-installer',
        icon: 'drive-harddisk-symbolic'
    },
    {
        name: 'Builder',
        subtitle: 'Rebuild ZenithOS from inside the ISO',
        command: 'zenith-build',
        icon: 'applications-engineering-symbolic'
    }
];

const ZENITH_QUICK_ACTIONS = [
    {
        name: 'Updates',
        command: ['zenith-terminal', '--command', 'apt list --upgradable; echo; flatpak remote-ls --updates 2>/dev/null || true; exec bash'],
        icon: 'software-update-available-symbolic'
    },
    {
        name: 'Boot Logs',
        command: ['zenith-terminal', '--command', 'journalctl -b --no-pager | tail -260; exec bash'],
        icon: 'text-x-generic-symbolic'
    },
    {
        name: 'Build Check',
        command: ['zenith-terminal', '--command', 'cd /usr/src/zenithos && zenith-build check; exec bash'],
        icon: 'applications-engineering-symbolic'
    },
    {
        name: 'Storage',
        command: ['zenith-terminal', '--command', 'df -h /; echo; du -sh /var/cache/apt/archives /var/lib/apt/lists /var/lib/flatpak /var/lib/zenith-build 2>/dev/null; exec bash'],
        icon: 'drive-harddisk-symbolic'
    }
];

const ZENITH_SYSTEM_ACTIONS = [
    {
        name: 'Network',
        command: ['gnome-control-center', 'network'],
        icon: 'network-wireless-symbolic'
    },
    {
        name: 'Sound',
        command: ['gnome-control-center', 'sound'],
        icon: 'audio-volume-high-symbolic'
    },
    {
        name: 'Power',
        command: ['gnome-control-center', 'power'],
        icon: 'battery-good-symbolic'
    },
    {
        name: 'Lock',
        command: ['loginctl', 'lock-session'],
        icon: 'system-lock-screen-symbolic'
    },
    {
        name: 'Restart',
        command: ['systemctl', 'reboot'],
        icon: 'view-refresh-symbolic'
    },
    {
        name: 'Power Off',
        command: ['systemctl', 'poweroff'],
        icon: 'system-shutdown-symbolic'
    }
];

function themedIcon(name, size, styleClass) {
    return new St.Icon({
        gicon: new Gio.ThemedIcon({name}),
        icon_size: size,
        style_class: styleClass
    });
}

class ZenithIndicator extends PanelMenu.Button {
    constructor(extension) {
        super(0.0, 'ZenithShell');
        this._extension = extension;

        const row = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-panel-chip'
        });
        row.add_child(themedIcon('view-grid-symbolic', 16, 'zenith-panel-icon'));
        row.add_child(new St.Label({
            text: 'ZenithOS',
            y_align: Clutter.ActorAlign.CENTER,
            style_class: 'zenith-panel-title'
        }));
        this.add_child(row);

        this.menu.addAction('Open Zenith Launcher', () => this._extension.toggleLauncher());
        this.menu.addAction('Activities Overview', () => Main.overview.toggle());
        for (const app of ZENITH_APPS) {
            this.menu.addAction(app.name, () => this._extension.launch(app.command));
        }
    }
}

class ZenithSystemMenu extends PanelMenu.Button {
    constructor(extension) {
        super(0.0, 'Zenith System');
        this._extension = extension;

        const row = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-system-chip'
        });
        row.add_child(themedIcon('emblem-system-symbolic', 16, 'zenith-system-chip-icon'));
        row.add_child(new St.Label({
            text: extension.profileLabel(),
            y_align: Clutter.ActorAlign.CENTER,
            style_class: 'zenith-system-chip-label'
        }));
        this.add_child(row);

        this.menu.addAction('Open Settings', () => this._extension.launch('zenith-settings'));
        this.menu.addAction('Open Packages', () => this._extension.launch('zenith-packages'));
        this.menu.addAction('Network Settings', () => this._extension.launch(['gnome-control-center', 'network']));
        this.menu.addAction('Sound Settings', () => this._extension.launch(['gnome-control-center', 'sound']));
        this.menu.addAction('Power Settings', () => this._extension.launch(['gnome-control-center', 'power']));
        this.menu.addAction('Lock Screen', () => this._extension.launch(['loginctl', 'lock-session']));
        this.menu.addAction('Restart', () => this._extension.launch(['systemctl', 'reboot']));
        this.menu.addAction('Power Off', () => this._extension.launch(['systemctl', 'poweroff']));
    }
}

export default class ZenithShellExtension extends Extension {
    enable() {
        this._signals = [];
        this._launcherVisible = false;
        this._profile = this._readProfile();

        this._indicator = new ZenithIndicator(this);
        Main.panel.addToStatusArea('zenith-shell', this._indicator, 0, 'center');

        this._systemMenu = new ZenithSystemMenu(this);
        Main.panel.addToStatusArea('zenith-system', this._systemMenu, 0, 'right');

        this._buildLauncher();
        this._buildDock();
        this._layoutActors();

        this._signals.push([
            Main.layoutManager,
            Main.layoutManager.connect('monitors-changed', () => this._layoutActors())
        ]);
        this._signals.push([
            global.display,
            global.display.connect('window-created', () => GLib.timeout_add(GLib.PRIORITY_DEFAULT, 180, () => {
                this._refreshTasks();
                return GLib.SOURCE_REMOVE;
            }))
        ]);
        this._signals.push([
            global.display,
            global.display.connect('restacked', () => this._refreshTasks())
        ]);
    }

    disable() {
        for (const [object, id] of this._signals ?? []) {
            object.disconnect(id);
        }
        this._signals = [];

        this._indicator?.destroy();
        this._indicator = null;

        this._systemMenu?.destroy();
        this._systemMenu = null;

        this._dock?.destroy();
        this._dock = null;

        this._launcher?.destroy();
        this._launcher = null;
        this._launcherPanel = null;
        this._tasksList = null;
        this._launcherVisible = false;
    }

    profileLabel() {
        return this._profile.name || this._profile.id || 'Workstation';
    }

    launch(command) {
        this.hideLauncher();
        const argv = Array.isArray(command) ? command : [command];
        try {
            Gio.Subprocess.new(argv, Gio.SubprocessFlags.NONE);
        } catch (error) {
            Main.notify('ZenithOS', `${argv[0]} is not available yet.`);
        }
    }

    toggleLauncher() {
        if (this._launcherVisible) {
            this.hideLauncher();
        } else {
            this.showLauncher();
        }
    }

    showLauncher() {
        if (!this._launcher) {
            return;
        }
        this._launcherVisible = true;
        this._profile = this._readProfile();
        this._refreshTasks();
        this._launcher.show();
        this._launcher.ease({
            opacity: 255,
            duration: 120,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD
        });
        this._launcher.grab_key_focus();
    }

    hideLauncher() {
        if (!this._launcher || !this._launcherVisible) {
            return;
        }
        this._launcherVisible = false;
        this._launcher.ease({
            opacity: 0,
            duration: 90,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => this._launcher?.hide()
        });
    }

    _buildDock() {
        this._dock = new St.BoxLayout({
            vertical: false,
            reactive: true,
            style_class: 'zenith-dock'
        });

        const launcherButton = this._dockButton('view-grid-symbolic', 'Launcher', () => this.toggleLauncher());
        launcherButton.add_style_class_name('zenith-dock-button-primary');
        this._dock.add_child(launcherButton);

        for (const app of ZENITH_APPS.slice(0, 5)) {
            this._dock.add_child(this._dockButton(app.icon, app.name, () => this.launch(app.command)));
        }

        this._dock.add_child(this._dockSeparator());
        this._dock.add_child(this._dockButton('view-app-grid-symbolic', 'Overview', () => Main.overview.toggle()));

        Main.layoutManager.addChrome(this._dock, {
            affectsStruts: false,
            trackFullscreen: false
        });
    }

    _dockButton(iconName, label, callback) {
        const button = new St.Button({
            style_class: 'zenith-dock-button',
            can_focus: true,
            track_hover: true,
            reactive: true
        });

        const box = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-dock-button-content'
        });
        box.add_child(themedIcon(iconName, 28, 'zenith-dock-icon'));
        box.add_child(new St.Label({
            text: label,
            style_class: 'zenith-dock-label'
        }));

        button.set_child(box);
        button.connect('clicked', callback);
        return button;
    }

    _dockSeparator() {
        return new St.Widget({
            style_class: 'zenith-dock-separator'
        });
    }

    _buildLauncher() {
        this._launcher = new St.Widget({
            style_class: 'zenith-launcher-backdrop',
            reactive: true,
            can_focus: true,
            visible: false,
            opacity: 0
        });

        this._launcher.connect('key-press-event', (_actor, event) => {
            if (event.get_key_symbol() === Clutter.KEY_Escape) {
                this.hideLauncher();
                return Clutter.EVENT_STOP;
            }
            return Clutter.EVENT_PROPAGATE;
        });
        this._launcher.connect('button-press-event', (_actor, event) => {
            if (event.get_source() === this._launcher) {
                this.hideLauncher();
                return Clutter.EVENT_STOP;
            }
            return Clutter.EVENT_PROPAGATE;
        });

        this._launcherPanel = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-launcher-panel'
        });

        const header = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-launcher-header'
        });
        const titleBlock = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            style_class: 'zenith-launcher-title-block'
        });
        titleBlock.add_child(new St.Label({
            text: 'ZenithOS',
            style_class: 'zenith-launcher-title'
        }));
        titleBlock.add_child(new St.Label({
            text: 'Workstation overview',
            style_class: 'zenith-launcher-subtitle'
        }));
        header.add_child(titleBlock);
        header.add_child(this._profilePill());
        this._launcherPanel.add_child(header);

        const search = new St.Button({
            style_class: 'zenith-search-row',
            can_focus: true,
            track_hover: true,
            reactive: true
        });
        const searchBox = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-search-row-content'
        });
        searchBox.add_child(themedIcon('system-search-symbolic', 18, 'zenith-search-icon'));
        searchBox.add_child(new St.Label({
            text: 'Open Activities for full search',
            style_class: 'zenith-search-label'
        }));
        search.set_child(searchBox);
        search.connect('clicked', () => {
            this.hideLauncher();
            Main.overview.toggle();
        });
        this._launcherPanel.add_child(search);

        const quick = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-quick-actions'
        });
        for (const action of ZENITH_QUICK_ACTIONS) {
            quick.add_child(this._quickActionButton(action));
        }
        this._launcherPanel.add_child(quick);

        const system = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-system-actions'
        });
        for (const action of ZENITH_SYSTEM_ACTIONS) {
            system.add_child(this._systemActionButton(action));
        }
        this._launcherPanel.add_child(system);

        const tasksGroup = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-tasks-group'
        });
        tasksGroup.add_child(new St.Label({
            text: 'Running windows',
            style_class: 'zenith-section-title'
        }));
        this._tasksList = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-tasks-list'
        });
        tasksGroup.add_child(this._tasksList);
        this._launcherPanel.add_child(tasksGroup);

        const appTitle = new St.Label({
            text: 'Apps',
            style_class: 'zenith-section-title'
        });
        this._launcherPanel.add_child(appTitle);

        const grid = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-launcher-grid'
        });
        for (const app of ZENITH_APPS) {
            grid.add_child(this._launcherCard(app));
        }
        this._launcherPanel.add_child(grid);

        this._launcher.add_child(this._launcherPanel);
        Main.layoutManager.addChrome(this._launcher, {
            affectsStruts: false,
            trackFullscreen: true
        });
    }

    _launcherCard(app) {
        const button = new St.Button({
            style_class: 'zenith-launcher-card',
            can_focus: true,
            track_hover: true,
            reactive: true
        });

        const row = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-launcher-card-row'
        });
        row.add_child(themedIcon(app.icon, 34, 'zenith-launcher-card-icon'));

        const copy = new St.BoxLayout({
            vertical: true,
            style_class: 'zenith-launcher-card-copy'
        });
        copy.add_child(new St.Label({
            text: app.name,
            style_class: 'zenith-launcher-card-title'
        }));
        copy.add_child(new St.Label({
            text: app.subtitle,
            style_class: 'zenith-launcher-card-subtitle'
        }));
        row.add_child(copy);

        button.set_child(row);
        button.connect('clicked', () => this.launch(app.command));
        return button;
    }

    _quickActionButton(action) {
        const button = new St.Button({
            style_class: 'zenith-quick-action',
            can_focus: true,
            track_hover: true,
            reactive: true
        });

        const row = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-quick-action-row'
        });
        row.add_child(themedIcon(action.icon, 18, 'zenith-quick-action-icon'));
        row.add_child(new St.Label({
            text: action.name,
            style_class: 'zenith-quick-action-label'
        }));

        button.set_child(row);
        button.connect('clicked', () => this.launch(action.command));
        return button;
    }

    _systemActionButton(action) {
        const button = new St.Button({
            style_class: 'zenith-system-action',
            can_focus: true,
            track_hover: true,
            reactive: true
        });

        const row = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-system-action-row'
        });
        row.add_child(themedIcon(action.icon, 18, 'zenith-system-action-icon'));
        row.add_child(new St.Label({
            text: action.name,
            style_class: 'zenith-system-action-label'
        }));

        button.set_child(row);
        button.connect('clicked', () => this.launch(action.command));
        return button;
    }

    _profilePill() {
        const pill = new St.BoxLayout({
            vertical: false,
            style_class: 'zenith-profile-pill'
        });
        pill.add_child(themedIcon('computer-symbolic', 16, 'zenith-profile-icon'));
        pill.add_child(new St.Label({
            text: `${this.profileLabel()} profile`,
            style_class: 'zenith-profile-label'
        }));
        return pill;
    }

    _readProfile() {
        const profile = {
            id: 'workstation',
            name: 'Workstation'
        };
        const path = '/run/zenith/hardware-profile.env';
        try {
            const [, contents] = GLib.file_get_contents(path);
            const text = new TextDecoder().decode(contents);
            for (const line of text.split('\n')) {
                const match = line.match(/^([A-Z0-9_]+)='?(.*?)'?$/);
                if (!match) {
                    continue;
                }
                if (match[1] === 'ZENITH_PROFILE_ID') {
                    profile.id = match[2];
                } else if (match[1] === 'ZENITH_PROFILE_NAME') {
                    profile.name = match[2];
                }
            }
        } catch (error) {
            // The detector may not have run yet during early session startup.
        }
        return profile;
    }

    _windowRows() {
        const rows = [];
        const actors = global.get_window_actors?.() ?? [];
        for (const actor of actors) {
            const win = actor.meta_window;
            if (!win || win.skip_taskbar || win.minimized) {
                continue;
            }
            const title = win.get_title?.() || 'Window';
            rows.push({title, window: win});
        }
        return rows.slice(0, 4);
    }

    _refreshTasks() {
        if (!this._tasksList) {
            return;
        }
        for (const child of this._tasksList.get_children()) {
            child.destroy();
        }
        const windows = this._windowRows();
        if (windows.length === 0) {
            const empty = new St.Label({
                text: 'No active windows',
                style_class: 'zenith-task-empty'
            });
            this._tasksList.add_child(empty);
            return;
        }

        for (const item of windows) {
            const button = new St.Button({
                style_class: 'zenith-task-row',
                can_focus: true,
                track_hover: true,
                reactive: true
            });
            const row = new St.BoxLayout({
                vertical: false,
                style_class: 'zenith-task-row-content'
            });
            row.add_child(themedIcon('window-symbolic', 17, 'zenith-task-icon'));
            row.add_child(new St.Label({
                text: item.title,
                style_class: 'zenith-task-title'
            }));
            button.set_child(row);
            button.connect('clicked', () => {
                this.hideLauncher();
                Main.activateWindow(item.window);
            });
            this._tasksList.add_child(button);
        }
    }

    _layoutActors() {
        const monitor = Main.layoutManager.primaryMonitor;
        if (!monitor) {
            return;
        }

        if (this._launcher) {
            this._launcher.set_position(monitor.x, monitor.y);
            this._launcher.set_size(monitor.width, monitor.height);
        }

        if (this._launcherPanel) {
            const panelWidth = Math.min(820, Math.floor(monitor.width * 0.74));
            const panelHeight = Math.min(760, Math.floor(monitor.height * 0.86));
            this._launcherPanel.set_size(panelWidth, panelHeight);
            this._launcherPanel.set_position(
                monitor.x + Math.floor((monitor.width - panelWidth) / 2),
                monitor.y + Math.floor((monitor.height - panelHeight) / 2)
            );
        }

        if (this._dock) {
            const dockWidth = Math.min(860, monitor.width - 48);
            const dockHeight = 78;
            this._dock.set_size(dockWidth, dockHeight);
            this._dock.set_position(
                monitor.x + Math.floor((monitor.width - dockWidth) / 2),
                monitor.y + monitor.height - dockHeight - 22
            );
        }
    }
}
