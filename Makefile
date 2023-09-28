VERSION = 0.1.0-dev

CXXFLAGS ?= -O3 -Wall -Wextra -Wshadow -pedantic
ST_CXXFLAGS = -std=c++2a
ST_CPPFLAGS = -I code/include -DVERSION=\"$(VERSION)\"

prefix = /usr/local
bindir = $(prefix)/bin

st-sources = code/src/main.cpp code/src/netlink.cpp code/src/output.cpp code/src/utils.cpp code/src/proc.cpp code/src/track.cpp
st-objs = $(st-sources:.cpp=.o)

.PHONY: all install install-setuid clean

all: ste

.cpp.o:
	$(CXX) $(CXXFLAGS) $(ST_CXXFLAGS) $(ST_CPPFLAGS) -c $< -o $@

ste: $(st-objs)
	$(CXX) $(LDFLAGS) -o $@ $(st-objs)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	cp -p ste $(DESTDIR)$(bindir)

install-setuid: install
	chown root:root $(DESTDIR)$(bindir)/ste
	chmod u+s $(DESTDIR)$(bindir)/ste

clean:
	rm -f $(st-objs) ste
