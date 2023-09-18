clang++ -fsanitize=address --std=c++2a -O3 -o st -I code/include code/src/main.cpp code/src/netlink.cpp code/src/output.cpp code/src/proc.cpp code/src/track.cpp code/src/utils.cpp
