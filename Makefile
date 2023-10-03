#---------------------------------------------------------------------------------
# VARIABLES
#---------------------------------------------------------------------------------

#Compiler and Linker
#CXX	  := clang++

#The Target Binary
TARGET	  := ste

#The Directories, Source, Includes, Objects, and Binary
SRCDIR	  := code/src
INCDIR	  := code/include
BUILDDIR  := obj
TARGETDIR := bin
SRCEXT	  := cpp
OBJEXT	  := o
INSTALLDIR:= /usr/local/bin

#Flags, Libraries and Includes
_CFLAGS	  := -Wall -O3 -g
_CXXFLAGS := -std=c++2a
INCLUDE   := -I$(INCDIR)
VERSION   := 0.1.0-dev

#---------------------------------------------------------------------------------
# BUSINESS LOGIC
#---------------------------------------------------------------------------------
SOURCES	 := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS	 := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

#Default task
all: dirs $(TARGET)

#Make the Directories
dirs:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

#Full Clean, Objects and Binaries
clean:
	@$(RM) -rf $(BUILDDIR)
	@$(RM) -rf $(TARGETDIR)

#Link
$(TARGET): $(OBJECTS)
	$(CXX) -o $(TARGETDIR)/$(TARGET) $(LDFLAGS) $^

#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CXX) -D VERSION='"$(VERSION)"' $(CXXFLAGS) $(_CFLAGS) $(_CXXFLAGS) $(INCLUDE) -c -o $@ $<

# Install with set-user-id
install: all
	sudo chown root $(TARGETDIR)/$(TARGET)
	sudo chmod +s $(TARGETDIR)/$(TARGET)
	sudo cp $(TARGETDIR)/$(TARGET) $(INSTALLDIR)

#Non-File Targets
.PHONY: all clean dirs