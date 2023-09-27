CXXFLAGS ?= -O3
ST_CXXFLAGS = -std=c++2a -Wall -Wextra
ST_CPPFLAGS = -I code/include

prefix = /usr/local
bindir = $(prefix)/bin

.PHONY: all install clean

all: st

.cpp.o:
	$(CXX) $(CXXFLAGS) $(ST_CXXFLAGS) $(ST_CPPFLAGS) -c $< -o $@

st-sources = $(wildcard code/src/*.cpp)
st-objs = $(st-sources:.cpp=.o)
st: $(st-objs)
	$(CXX) $(LDFLAGS) -o $@ $(st-objs)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	cp -p st $(DESTDIR)$(bindir)

clean:
	rm -f $(st-objs) st
