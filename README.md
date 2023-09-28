# ste: Space-Time Explorer

`ste` is a tool to [explore how command-line program work](https://fabiensanglard.net/st/index.html). It collects PSS usage over time, process/thread spawned, and wall-time.

## sudo

Since `ste` uses netlink process monitoring, it needs to be run with `sudo`.

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
