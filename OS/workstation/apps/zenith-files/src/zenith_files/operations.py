import os
import shutil
from datetime import datetime
from urllib.parse import quote


def _decode_mount_path(value):
    return (
        value
        .replace("\\040", " ")
        .replace("\\011", "\t")
        .replace("\\012", "\n")
        .replace("\\134", "\\")
    )


def unique_destination(path):
    if not os.path.exists(path):
        return path

    parent = os.path.dirname(path)
    name = os.path.basename(path)
    stem, extension = os.path.splitext(name)
    for index in range(2, 1000):
        suffix = " copy" if index == 2 else f" copy {index - 1}"
        candidate = os.path.join(parent, f"{stem}{suffix}{extension}")
        if not os.path.exists(candidate):
            return candidate
    raise FileExistsError(f"could not choose a free destination for {path}")


def _reject_folder_into_self(source, destination_dir, operation):
    if not os.path.isdir(source):
        return

    source_real = os.path.realpath(source)
    destination_real = os.path.realpath(destination_dir)
    try:
        if os.path.commonpath([source_real, destination_real]) == source_real:
            raise ValueError(f"cannot {operation} a folder into itself")
    except ValueError:
        raise


def copy_path(source, destination_dir):
    if not os.path.exists(source):
        raise FileNotFoundError(source)
    if not os.path.isdir(destination_dir):
        raise NotADirectoryError(destination_dir)

    _reject_folder_into_self(source, destination_dir, "copy")
    destination = unique_destination(os.path.join(destination_dir, os.path.basename(source)))
    if os.path.isdir(source):
        shutil.copytree(source, destination, symlinks=True)
    else:
        shutil.copy2(source, destination)
    return destination


def move_path(source, destination_dir):
    if not os.path.exists(source):
        raise FileNotFoundError(source)
    if not os.path.isdir(destination_dir):
        raise NotADirectoryError(destination_dir)

    _reject_folder_into_self(source, destination_dir, "move")
    destination = unique_destination(os.path.join(destination_dir, os.path.basename(source)))
    shutil.move(source, destination)
    return destination


def trash_path(path, home=None):
    if not os.path.exists(path):
        raise FileNotFoundError(path)

    home = home or os.path.expanduser("~")
    trash_root = os.path.join(home, ".local", "share", "Trash")
    files_dir = os.path.join(trash_root, "files")
    info_dir = os.path.join(trash_root, "info")
    os.makedirs(files_dir, exist_ok=True)
    os.makedirs(info_dir, exist_ok=True)

    original = os.path.abspath(path)
    destination = unique_destination(os.path.join(files_dir, os.path.basename(path)))
    shutil.move(path, destination)

    info_name = os.path.basename(destination) + ".trashinfo"
    info_path = os.path.join(info_dir, info_name)
    deletion_date = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
    with open(info_path, "w", encoding="utf-8") as handle:
        handle.write("[Trash Info]\n")
        handle.write(f"Path={quote(original)}\n")
        handle.write(f"DeletionDate={deletion_date}\n")

    return destination


def parse_mountinfo(text):
    volumes = []
    seen = set()
    for line in text.splitlines():
        fields = line.split()
        if "-" not in fields:
            continue

        separator = fields.index("-")
        if len(fields) <= separator + 2 or len(fields) <= 4:
            continue

        mount_point = _decode_mount_path(fields[4])
        fs_type = fields[separator + 1]
        source = fields[separator + 2]

        if fs_type in {"autofs", "binfmt_misc", "bpf", "cgroup2", "debugfs", "devpts", "devtmpfs", "efivarfs", "fusectl", "mqueue", "proc", "pstore", "securityfs", "sysfs", "tmpfs", "tracefs"}:
            continue

        is_user_volume = (
            mount_point == "/"
            or mount_point == "/boot/efi"
            or mount_point.startswith(("/media/", "/mnt/", "/run/media/"))
        )
        if not is_user_volume or mount_point in seen:
            continue

        seen.add(mount_point)
        label = "Root filesystem" if mount_point == "/" else os.path.basename(mount_point) or mount_point
        volumes.append((label, mount_point, source, fs_type))
    return volumes


def mounted_volumes(mountinfo_path="/proc/self/mountinfo"):
    try:
        with open(mountinfo_path, "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError:
        return []
    return parse_mountinfo(text)
