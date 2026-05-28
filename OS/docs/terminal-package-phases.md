# ZenithOS Terminal and Package Phases

## Phase 0

- Status: PASS
- Artifact: `userland/terminal.c.audit`
- Result: documented the existing 28 syscalls, command surface, IPC registry, and terminal binary budget.

## Phase 1

- Status: PASS
- Artifact: `userland/terminal.c.phase1.verify`
- Result: terminal now uses a tokenizer, command table, aliases, env table, command history, tab completion, line editing, and bridge-only package commands.
- Persistence decision: history is persisted by the session process through `Z_UI_CMD_BRIDGE_DATA` frames. This avoids new syscalls and respects the read-only initramfs.
- Binary: `build/userland/terminal.bin` was 16026 bytes after Phase 1.

## Phase 2

- Status: PARTIAL PASS
- Syscalls: `SYS_PIPE=28`, `SYS_DUP2=29`, `SYS_GETPID=30`, `SYS_GETPPID=31`
- Result: kernel has static FD tables, memory-backed files, 4096-byte kernel pipe buffers, `dup2`, and PID queries. Terminal supports `>`, `>>`, `<`, and `|` for built-in commands.
- Important limitation: the exact POSIX `fork -> dup2 -> exec -> wait` pipeline cannot be completed until ZenithOS has a fork or process-clone syscall. This phase deliberately did not add an unrequested `fork` syscall.

## Phase 3

- Status: PASS with signing prerequisite
- Result: added `zpkg` with `install`, `remove`, `update`, `search`, `list`, `info`, and `self-test`.
- Resolver: dependency graph resolver detects cycles and handles diamond and long-chain graphs.
- Deb support: `zpkg` parses ar-format `.deb` archives and extracts `data.tar.*` using Python stdlib `tarfile`.
- Transaction support: package state and dpkg status are snapshotted before mutation and restored on failure.
- Repository publishing: `publish-apt-repo.sh` now emits `Packages`, `Packages.gz`, `Release`, `InRelease`, `Release.gpg`, and `zenith-archive-keyring.gpg`.
- Signing gate: publishing now fails unless `ZENITH_APT_SIGNING_KEY` names a local GPG secret key.

## Phase 4

- Status: PASS by static verification
- Result: Zenith Terminal now has VTE PTY sessions, tabs, split panes, profile loading, fail-closed JSON profile fallback, 1000-10000 line scrollback bounds, URL matching when VTE exposes regex support, broadcast input, and an ANSI CSI state machine.
- Constraint: GTK/VTE runtime launch was not executed from the Windows shell; Python AST validation passed.

## Phase 5

- Status: PARTIAL PASS
- Syscalls: `SYS_REBOOT=32`, `SYS_CHDIR=33`, `SYS_GETCWD=34`
- Result: terminal `REBOOT`, `CD`, and `PWD` now use kernel syscalls. `REBOOT` uses the x86 keyboard-controller reset path.
- Build verification: `mingw32-make all` passed, `mingw32-make iso` passed, final `build/userland/terminal.bin` is 23964 bytes.
- Remaining gate: byte-level bridge E2E verification and live GTK/VTE stress testing need a QEMU/desktop runtime harness.

## Verification Commands Run

```text
mingw32-make build/userland/terminal.bin
mingw32-make build/userland/session.bin
mingw32-make all
mingw32-make iso
python workstation/apps/zenith-packages/src/zenith_packages/zpkg.py self-test
python -c "import ast, pathlib; [ast.parse(pathlib.Path(p).read_text(encoding='utf-8')) for p in [...]]"
rg "#include|malloc|calloc|realloc|free|printf|puts|strcpy|strlen|string\.h|stdio\.h|stdlib\.h" userland/terminal.c
```
