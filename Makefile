CXXFLAGS ?= -O3 -Wall -Wextra
ST_CXXFLAGS = -std=c++2a
ST_CPPFLAGS = -I code/include

prefix = /usr/local
bindir = $(prefix)/bin

st-sources = $(wildcard code/src/*.cpp)
st-objs = $(st-sources:.cpp=.o)

.PHONY: all install clean

all: st

.cpp.o:
	$(CXX) $(CXXFLAGS) $(ST_CXXFLAGS) $(ST_CPPFLAGS) -c $< -o $@

st: $(st-objs)
	$(CXX) $(LDFLAGS) -o $@ $(st-objs)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	cp -p st $(DESTDIR)$(bindir)

clean:
	rm -f $(st-objs) st
