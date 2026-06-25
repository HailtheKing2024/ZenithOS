import re


class PackageCommandError(ValueError):
    pass


_APT_PACKAGE_RE = re.compile(r"^[a-z0-9][a-z0-9+.-]*$")
_FLATPAK_APP_ID_RE = re.compile(r"^[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+){2,}$")


def validate_apt_package(value):
    package = value.strip()
    if not _APT_PACKAGE_RE.fullmatch(package):
        raise PackageCommandError("APT package names may contain lowercase letters, numbers, '+', '.', and '-'")
    return package


def validate_flatpak_app_id(value):
    app_id = value.strip()
    if not _FLATPAK_APP_ID_RE.fullmatch(app_id):
        raise PackageCommandError("Flatpak app IDs must look like org.example.Application")
    return app_id


def apt_command(action, package):
    package = validate_apt_package(package)
    if action == "install":
        return ["pkexec", "apt-get", "install", "-y", package]
    if action == "remove":
        return ["pkexec", "apt-get", "remove", "-y", package]
    raise PackageCommandError(f"unsupported APT action: {action}")


def flatpak_command(action, app_id):
    app_id = validate_flatpak_app_id(app_id)
    if action == "install":
        return ["flatpak", "install", "-y", "flathub", app_id]
    if action == "remove":
        return ["flatpak", "uninstall", "-y", app_id]
    raise PackageCommandError(f"unsupported Flatpak action: {action}")
