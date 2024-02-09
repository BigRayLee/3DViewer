CC := g++

VPATH = .:src:extern/mesh_simplify:extern/miniply:extern/fast_obj:extern/imgui

SRC := $(wildcard src/*.cpp)
SRC := $(SRC) extern/miniply/miniply.cpp 
SRC := $(SRC) $(wildcard extern/imgui/*.cpp)
SRC := $(SRC) extern/mesh_simplify/simplifier_mod.cpp 
SRC := $(SRC) extern/mesh_simplify/indexgenerator.cpp 
SRC := $(SRC) extern/mesh_simplify/simplify_mod_tex.cpp
SRC := $(SRC) extern/mesh_simplify/vcacheanalyzer.cpp
SRC := $(SRC) extern/mesh_simplify/vcacheoptimizer.cpp
SRC := $(SRC) extern/mesh_simplify/overdrawoptimizer.cpp
SRC := $(SRC) extern/mesh_simplify/vfetchoptimizer.cpp

INCLUDE = -I include -I extern -I usr/include/glad -I usr/include/GLES -I usr/include/GLES2
OBJDIR := obj
BINDIR := bin

ifdef DEBUG
	CFLAGS := -O2 -march=x86-64 -g -Wall -Wno-format -Wno-unused-variable -pthread -fopenmp
	OBJDIR := $(OBJDIR)/debug
	BINDIR := $(BINDIR)/debug
else
	CFLAGS := -O2 -march=x86-64 -DNDEBUG 
endif

DEPDIR := $(OBJDIR)/.deps
DEPSFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

OBJECTS  := $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SRC)))
DEPFILES := $(patsubst %.cpp, $(DEPDIR)/%.d, $(notdir $(SRC)))


LDFLAGS = -lGL -lglfw -ldl -pthread 
CXXFLAGS= -Wall -Wextra -Wpedantic -Wformat -std=c++17 -pthread  

APP = $(BINDIR)/viewer

#------------------------------------------------------------------------------
.PHONY: default

default: $(APP) 

$(APP): $(OBJECTS) | $(BINDIR)
	@echo "Building $(APP)."
	@$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $(APP)

$(BINDIR) :
	@echo "Creating directory " $(BINDIR)
	@mkdir -p $(BINDIR)

$(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.d | $(OBJDIR) $(DEPDIR)
	@echo "Compiling "$<
	@$(CC) $(DEPSFLAGS) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(OBJDIR) :
	@echo "Creating directory " $(OBJDIR)
	@mkdir -p $(OBJDIR)

$(DEPDIR): 
	@mkdir -p $@

$(DEPFILES):
include $(wildcard $(DEPFILES))

.PHONY : clean 
clean :
	@rm -f $(OBJECTS)

