CXXFLAGS ?= -O3 -Wall -Wextra -Wshadow -pedantic
ST_CXXFLAGS = -std=c++2a
ST_CPPFLAGS = -I code/include

prefix = /usr/local
bindir = $(prefix)/bin

st-sources = code/src/main.cpp code/src/netlink.cpp code/src/output.cpp code/src/utils.cpp code/src/proc.cpp code/src/track.cpp
st-objs = $(st-sources:.cpp=.o)

.PHONY: all install clean

all: ste

.cpp.o:
	$(CXX) $(CXXFLAGS) $(ST_CXXFLAGS) $(ST_CPPFLAGS) -c $< -o $@

ste: $(st-objs)
	$(CXX) $(LDFLAGS) -o $@ $(st-objs)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	cp -p st $(DESTDIR)$(bindir)

clean:
	rm -f $(st-objs) ste
