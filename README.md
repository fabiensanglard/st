# ste: Space-Time Explorer

`ste` is a tool to [explore how command-line program work](https://fabiensanglard.net/st/index.html). It collects PSS usage over time, process/thread spawned, and wall-time.

## Example

```
$ sudo ste clang -o hello hello.c

EXEC: [clang /home/leaf/hello.cc]
EXEC: [/usr/lib/llvm-14/bin/clang -cc1 ...]
EXEC: [/usr/bin/ld ...]
Num threads = 3
Num process = 3
Max PSS: 127,156,224 bytes
Walltime: 221ms - user-space: 136ms - kernel-space: 69ms
```

```
127┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
   ┃                                                                                     ┃
   ┃                                                                                     ┃
   ┃                                                          █████████████              ┃
   ┃            █                    ██████████████████████████████████████              ┃
   ┃            █           ███████████████████████████████████████████████              ┃
   ┃            █         █████████████████████████████████████████████████      █ ████  ┃
   ┃            █  ████████████████████████████████████████████████████████  ████████████┃
   ┫            ███████████████████████████████████████████████████████████ █████████████┃
   ┃           ██████████████████████████████████████████████████████████████████████████┃
   ┃          ███████████████████████████████████████████████████████████████████████████┃
   ┃        █████████████████████████████████████████████████████████████████████████████┃
   ┃      ███████████████████████████████████████████████████████████████████████████████┃
   ┃    █████████████████████████████████████████████████████████████████████████████████┃
   ┃   ██████████████████████████████████████████████████████████████████████████████████┃
   ┃█████████████████████████████████████████████████████████████████████████████████████┃
0MB┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
   0ms                                                                                 221
```


## Installation

Release build:

```
make
make install prefix=<prefix>
```

For debug builds, specify `CXXFLAGS` and `LDFLAGS` before running make:

```
export CXXFLAGS="-Og -fsanitize=address" LDFLAGS=-fsanitize=address
```

## sudo or setuid

Since `ste` uses netlink process monitoring, it needs to be run with root
privileges. There are two ways to do so:

1. Run `sudo ste ...`
2. Install `ste` as a setuid executable:
   ```
   make
   sudo make install-setuid prefix=<prefix>
   ```

The setuid executable approach is convenient when you need to trace e.g.
Python scripts, or programs that need to access the home folder of the current,
non-root user.

Notice that root privileges are dropped before executing the command that is
traced.