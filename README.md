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

Check your g++ version. If you are running "old" versions (gcc7 etc. is the standard on may distros), you will get compile time errors, and the solution suggested by the compiler (eg. use ST_CXXFLAGS = -std=c++11 in your Makefile) will cause further errors. Because the code uses constructs like std::map::contains(), you need >= C++20, so option C++11 will fail. The the Makefile calls for C++2a, which is fine.
The solution is to install addtional gcc packages (in parallel), and then use alternatives to manage the versions for compatibility.
For example, on OpenSUSE:
```
> which g++
/usr/bin/g++
> zypper install gcc11 gcc11-c++
> alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 1
> alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 99
```
Then select to use g++-11 for the build, without disturbing your default (in our example, g++-7)
```
> update-alternatives --config g++
There are 2 choices for the alternative g++ (providing /usr/bin/g++).

  Selection    Path             Priority   Status
------------------------------------------------------------
* 0            /usr/bin/g++-7    99        auto mode
  1            /usr/bin/g++-11   1         manual mode
  2            /usr/bin/g++-7    99        manual mode

Press <enter> to keep the current choice[*], or type selection number: 1   
update-alternatives: using /usr/bin/g++-11 to provide /usr/bin/g++ (g++) in manual mode
```
The solution on Ubuntu and other Deb-based distributions is similar:
Run the following command to add the Toolchain repository:
```
> sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
```
Install gcc 11:
```
> sudo apt install -y gcc-11
```
Check gcc version to verify that the installation completed successfully:
```
> gcc-11 --version
```
Then use update-alternatives with the same result as with OpenSUSE above:
```
> sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 1
> sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 99

```

Release build:

```
> make
> make install prefix=<prefix>
```
You can specify a directory <prefix> (being the root directory that you wish to install the binary into, eg. "/usr/local").
The Makefile will append "/bin" to the prefix, to get the full install directory for the "ste" binary (eg. /usr/local/bin)

Uninstall:
```
> make uninstall prefix=<prefix>
```
Specify the same prefix as you used to install. You will be prompted to confirm deleting the binary.
NB! The "bin" directory will remain. You can mod the Makefile to also do a rmdir, which will fail safe if the bin directory contains other files.

For debug builds, specify `CXXFLAGS` and `LDFLAGS` before running make:

```
export CXXFLAGS="-Og -fsanitize=address" LDFLAGS=-fsanitize=address
```
And yes, CXXFLAGS is correct, being the flags for the C++ compiler. CFLAGS is for the C compiler, and CPPFLAGS is for the C preprocessor.
