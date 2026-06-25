import os
import tempfile
import unittest

from zenith_files.operations import copy_path, move_path, parse_mountinfo, trash_path


class FileOperationTests(unittest.TestCase):
    def test_copy_file_uses_unique_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            source_dir = os.path.join(tmp, "source")
            destination_dir = os.path.join(tmp, "destination")
            os.mkdir(source_dir)
            os.mkdir(destination_dir)
            source = os.path.join(source_dir, "note.txt")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write("zenith")
            with open(os.path.join(destination_dir, "note.txt"), "w", encoding="utf-8") as handle:
                handle.write("existing")

            copied = copy_path(source, destination_dir)

            self.assertEqual(os.path.basename(copied), "note copy.txt")
            with open(copied, "r", encoding="utf-8") as handle:
                self.assertEqual(handle.read(), "zenith")

    def test_move_folder_rejects_moving_into_itself(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = os.path.join(tmp, "folder")
            child = os.path.join(source, "child")
            os.makedirs(child)

            with self.assertRaises(ValueError):
                move_path(source, child)

    def test_trash_path_creates_trashinfo(self):
        with tempfile.TemporaryDirectory() as tmp:
            home = os.path.join(tmp, "home")
            os.mkdir(home)
            source = os.path.join(tmp, "delete-me.txt")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write("remove")

            trashed = trash_path(source, home=home)

            self.assertFalse(os.path.exists(source))
            self.assertTrue(os.path.exists(trashed))
            info_path = os.path.join(home, ".local", "share", "Trash", "info", "delete-me.txt.trashinfo")
            self.assertTrue(os.path.exists(info_path))

    def test_parse_mountinfo_returns_user_visible_volumes(self):
        mountinfo = "\n".join([
            "20 25 0:19 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw",
            "30 25 8:2 /@ / rw,noatime - btrfs /dev/vda2 rw",
            "31 30 8:1 / /boot/efi rw,relatime - vfat /dev/vda1 rw",
            "32 30 8:3 / /media/zenith/USB\\040Drive rw,relatime - ext4 /dev/sdb1 rw",
        ])

        volumes = parse_mountinfo(mountinfo)

        self.assertEqual(
            volumes,
            [
                ("Root filesystem", "/", "/dev/vda2", "btrfs"),
                ("efi", "/boot/efi", "/dev/vda1", "vfat"),
                ("USB Drive", "/media/zenith/USB Drive", "/dev/sdb1", "ext4"),
            ],
        )


if __name__ == "__main__":
    unittest.main()
