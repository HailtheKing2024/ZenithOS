import unittest

from zenith_packages.workers import (
    PackageCommandError,
    apt_command,
    flatpak_command,
    validate_apt_package,
    validate_flatpak_app_id,
)


class PackageWorkerCommandTests(unittest.TestCase):
    def test_validates_apt_package_names(self):
        self.assertEqual(validate_apt_package("plasma-discover"), "plasma-discover")
        self.assertEqual(validate_apt_package("libqt6core6t64"), "libqt6core6t64")

    def test_rejects_unsafe_apt_package_names(self):
        for value in ["", "vim;reboot", "../dpkg", "two words", "-starts-with-dash"]:
            with self.subTest(value=value):
                with self.assertRaises(PackageCommandError):
                    validate_apt_package(value)

    def test_validates_flatpak_app_ids(self):
        self.assertEqual(validate_flatpak_app_id("org.mozilla.firefox"), "org.mozilla.firefox")
        self.assertEqual(validate_flatpak_app_id("com.visualstudio.code"), "com.visualstudio.code")

    def test_rejects_unsafe_flatpak_app_ids(self):
        for value in ["", "firefox", "org.example", "org.example.bad/id", "org.example.bad id"]:
            with self.subTest(value=value):
                with self.assertRaises(PackageCommandError):
                    validate_flatpak_app_id(value)

    def test_builds_argv_only_apt_commands(self):
        self.assertEqual(
            apt_command("install", "plasma-discover"),
            ["pkexec", "apt-get", "install", "-y", "plasma-discover"],
        )
        self.assertEqual(
            apt_command("remove", "plasma-discover"),
            ["pkexec", "apt-get", "remove", "-y", "plasma-discover"],
        )

    def test_builds_argv_only_flatpak_commands(self):
        self.assertEqual(
            flatpak_command("install", "org.mozilla.firefox"),
            ["flatpak", "install", "-y", "flathub", "org.mozilla.firefox"],
        )
        self.assertEqual(
            flatpak_command("remove", "org.mozilla.firefox"),
            ["flatpak", "uninstall", "-y", "org.mozilla.firefox"],
        )


if __name__ == "__main__":
    unittest.main()
