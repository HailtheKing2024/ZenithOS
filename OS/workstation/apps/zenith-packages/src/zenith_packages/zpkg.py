import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
from dataclasses import dataclass
from pathlib import Path


DEFAULT_REPO_DIR = Path(os.environ.get("ZENITH_REPO_DIR", "/var/lib/zenith-repo"))
DEFAULT_ROOT = Path(os.environ.get("ZPKG_ROOT", "/"))
STATE_RELATIVE = Path("var/lib/zenith-zpkg/installed.json")
DPKG_STATUS_RELATIVE = Path("var/lib/dpkg/status")


@dataclass(frozen=True)
class PackageRecord:
    name: str
    version: str
    filename: str
    depends: tuple[str, ...]
    description: str


class ResolverError(RuntimeError):
    pass


class Transaction:
    def __init__(self, root: Path):
        self.root = root
        self._backups: list[tuple[Path, Path | None]] = []
        self._tmpdir: tempfile.TemporaryDirectory[str] | None = None

    def __enter__(self):
        self._tmpdir = tempfile.TemporaryDirectory(prefix="zpkg-txn-")
        self._snapshot(self.root / STATE_RELATIVE)
        self._snapshot(self.root / DPKG_STATUS_RELATIVE)
        return self

    def __exit__(self, exc_type, _exc, _tb):
        if exc_type is not None:
            self.rollback()
        if self._tmpdir is not None:
            self._tmpdir.cleanup()
        return False

    def _snapshot(self, path: Path):
        assert self._tmpdir is not None
        if path.exists():
            backup = Path(self._tmpdir.name) / path.relative_to(self.root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, backup)
            self._backups.append((path, backup))
        else:
            self._backups.append((path, None))

    def rollback(self):
        for path, backup in reversed(self._backups):
            if backup is None:
                try:
                    path.unlink()
                except FileNotFoundError:
                    pass
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(backup, path)


def parse_packages_file(packages_path: Path) -> dict[str, PackageRecord]:
    records: dict[str, PackageRecord] = {}
    if not packages_path.exists():
        return records

    fields: dict[str, str] = {}
    last_key: str | None = None

    def commit():
        if "Package" not in fields:
            return
        name = fields["Package"]
        records[name] = PackageRecord(
            name=name,
            version=fields.get("Version", ""),
            filename=fields.get("Filename", ""),
            depends=tuple(parse_depends(fields.get("Depends", ""))),
            description=fields.get("Description", ""),
        )

    for raw in packages_path.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            commit()
            fields = {}
            last_key = None
            continue
        if raw.startswith(" ") and last_key is not None:
            fields[last_key] = f"{fields[last_key]}\n{raw[1:]}"
            continue
        if ":" not in raw:
            continue
        key, value = raw.split(":", 1)
        fields[key] = value.strip()
        last_key = key

    commit()
    return records


def parse_depends(value: str) -> list[str]:
    names: list[str] = []
    for chunk in value.split(","):
        alt = chunk.strip().split("|", 1)[0].strip()
        if not alt:
            continue
        name = alt.split(" ", 1)[0].split(":", 1)[0].strip()
        if name:
            names.append(name)
    return names


class DependencyResolver:
    def __init__(self, records: dict[str, PackageRecord]):
        self.records = records
        self._state: dict[str, str] = {}
        self._order: list[str] = []
        self._stack: list[str] = []

    def resolve(self, roots: list[str]) -> list[PackageRecord]:
        for name in roots:
            self._visit(name)
        return [self.records[name] for name in self._order]

    def _visit(self, name: str):
        state = self._state.get(name)
        if state == "done":
            return
        if state == "visiting":
            cycle = self._stack[self._stack.index(name):] + [name]
            raise ResolverError("dependency cycle: " + " -> ".join(cycle))
        if name not in self.records:
            raise ResolverError(f"package not found: {name}")

        self._state[name] = "visiting"
        self._stack.append(name)
        for dep in self.records[name].depends:
            if dep in self.records:
                self._visit(dep)
        self._stack.pop()
        self._state[name] = "done"
        self._order.append(name)


def load_state(root: Path) -> dict[str, dict[str, object]]:
    state_path = root / STATE_RELATIVE
    if not state_path.exists():
        return {}
    try:
        value = json.loads(state_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def save_state(root: Path, state: dict[str, dict[str, object]]):
    state_path = root / STATE_RELATIVE
    state_path.parent.mkdir(parents=True, exist_ok=True)
    tmp = state_path.with_suffix(".tmp")
    tmp.write_text(json.dumps(state, indent=2, sort_keys=True), encoding="utf-8")
    tmp.replace(state_path)


def ar_members(path: Path):
    data = path.read_bytes()
    if not data.startswith(b"!<arch>\n"):
        raise RuntimeError(f"not a deb archive: {path}")
    offset = 8
    while offset + 60 <= len(data):
        header = data[offset:offset + 60]
        offset += 60
        name = header[0:16].decode("ascii", errors="ignore").strip().rstrip("/")
        size_text = header[48:58].decode("ascii", errors="ignore").strip()
        try:
            size = int(size_text)
        except ValueError as exc:
            raise RuntimeError(f"invalid ar member size in {path}") from exc
        payload = data[offset:offset + size]
        yield name, payload
        offset += size + (size % 2)


def unpack_deb_data(deb_path: Path, root: Path) -> list[str]:
    extracted: list[str] = []
    data_payload: bytes | None = None
    data_name = ""
    for name, payload in ar_members(deb_path):
        if name.startswith("data.tar"):
            data_payload = payload
            data_name = name
            break
    if data_payload is None:
        raise RuntimeError(f"data archive missing in {deb_path}")

    with tempfile.NamedTemporaryFile(suffix="-" + data_name, delete=False) as handle:
        handle.write(data_payload)
        temp_name = handle.name
    try:
        with tarfile.open(temp_name, mode="r:*") as archive:
            for member in archive.getmembers():
                if member.name.startswith("/") or ".." in Path(member.name).parts:
                    raise RuntimeError(f"unsafe path in {deb_path}: {member.name}")
                archive.extract(member, root)
                if member.isfile() or member.issym():
                    extracted.append(str(Path("/") / member.name))
    finally:
        Path(temp_name).unlink(missing_ok=True)
    return extracted


def repo_records(repo_dir: Path) -> dict[str, PackageRecord]:
    return parse_packages_file(repo_dir / "Packages")


def resolve_packages(repo_dir: Path, names: list[str]) -> list[PackageRecord]:
    return DependencyResolver(repo_records(repo_dir)).resolve(names)


def command_search(args) -> int:
    records = repo_records(args.repo)
    query = args.query.lower()
    for record in records.values():
        haystack = f"{record.name} {record.description}".lower()
        if query in haystack:
            print(f"{record.name}\t{record.version}\t{record.description}")
    return 0


def command_list(args) -> int:
    for record in repo_records(args.repo).values():
        print(f"{record.name}\t{record.version}")
    return 0


def command_info(args) -> int:
    records = repo_records(args.repo)
    record = records.get(args.name)
    if record is None:
        print(f"zpkg: package not found: {args.name}", file=sys.stderr)
        return 1
    print(f"Package: {record.name}")
    print(f"Version: {record.version}")
    print(f"Depends: {', '.join(record.depends) if record.depends else '-'}")
    print(f"Filename: {record.filename}")
    print(f"Description: {record.description}")
    return 0


def command_install(args) -> int:
    order = resolve_packages(args.repo, args.names)
    print("Install order: " + ", ".join(record.name for record in order))
    if args.dry_run:
        return 0
    if not args.yes:
        answer = input("Proceed with install? [y/N] ").strip().lower()
        if answer != "y":
            return 1

    state = load_state(args.root)
    with Transaction(args.root):
        for record in order:
            deb_path = args.repo / record.filename
            if not deb_path.exists():
                raise RuntimeError(f"missing deb: {deb_path}")
            files = unpack_deb_data(deb_path, args.root)
            state[record.name] = {
                "version": record.version,
                "files": files,
            }
        save_state(args.root, state)
    return 0


def command_remove(args) -> int:
    state = load_state(args.root)
    reverse = [
        name for name, record in repo_records(args.repo).items()
        if args.name in record.depends and name in state
    ]
    if reverse and not args.yes:
        print("Reverse dependencies installed: " + ", ".join(reverse), file=sys.stderr)
        return 1
    if args.name not in state:
        print(f"zpkg: package not installed: {args.name}", file=sys.stderr)
        return 1

    with Transaction(args.root):
        for item in reversed(state[args.name].get("files", [])):
            path = args.root / str(item).lstrip("/")
            try:
                path.unlink()
            except FileNotFoundError:
                pass
        del state[args.name]
        save_state(args.root, state)
    return 0


def command_update(args) -> int:
    source = args.root / "etc/apt/sources.list.d/zenith-local.list"
    source.parent.mkdir(parents=True, exist_ok=True)
    source.write_text(f"deb [signed-by=/var/lib/zenith-repo/zenith-archive-keyring.gpg] file://{args.repo} ./\n", encoding="utf-8")
    if args.run_apt:
        return subprocess.call(["apt-get", "update"])
    print(f"wrote {source}")
    return 0


def command_fixtures(_args) -> int:
    def records(edges: dict[str, list[str]]) -> dict[str, PackageRecord]:
        return {
            name: PackageRecord(name, "1", f"{name}.deb", tuple(deps), name)
            for name, deps in edges.items()
        }

    fixtures = {
        "A": ({"A": ["B"], "B": ["C"], "C": ["A"]}, True),
        "B": ({"A": ["B", "C"], "B": ["C"], "C": []}, False),
        "C": ({chr(ord("A") + i): ([chr(ord("A") + i + 1)] if i < 25 else []) for i in range(26)}, False),
    }
    for fixture_id, (graph, expect_cycle) in fixtures.items():
        try:
            DependencyResolver(records(graph)).resolve(["A"])
            actual_cycle = False
        except ResolverError:
            actual_cycle = True
        status = "PASS" if actual_cycle == expect_cycle else "FAIL"
        print(f"Fixture {fixture_id}: {status}")
        if status != "PASS":
            return 1
    chain = {f"pkg{i:02d}": ([f"pkg{i + 1:02d}"] if i < 49 else []) for i in range(50)}
    DependencyResolver(records(chain)).resolve(["pkg00"])
    print("Fixture 50-chain: PASS")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="zpkg")
    parser.add_argument("--repo", type=Path, default=DEFAULT_REPO_DIR)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    sub = parser.add_subparsers(dest="command", required=True)

    search = sub.add_parser("search")
    search.add_argument("query")
    search.set_defaults(func=command_search)

    list_cmd = sub.add_parser("list")
    list_cmd.set_defaults(func=command_list)

    info = sub.add_parser("info")
    info.add_argument("name")
    info.set_defaults(func=command_info)

    install = sub.add_parser("install")
    install.add_argument("--dry-run", action="store_true")
    install.add_argument("-y", "--yes", action="store_true")
    install.add_argument("names", nargs="+")
    install.set_defaults(func=command_install)

    remove = sub.add_parser("remove")
    remove.add_argument("-y", "--yes", action="store_true")
    remove.add_argument("name")
    remove.set_defaults(func=command_remove)

    update = sub.add_parser("update")
    update.add_argument("--run-apt", action="store_true")
    update.set_defaults(func=command_update)

    fixtures = sub.add_parser("self-test")
    fixtures.set_defaults(func=command_fixtures)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except ResolverError as exc:
        print(f"zpkg: {exc}", file=sys.stderr)
        return 2
    except RuntimeError as exc:
        print(f"zpkg: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
