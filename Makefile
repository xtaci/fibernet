.PHONY : all clean 

CFLAGS = -g -I./include -I/usr/include/lua5.2
LDFLAGS = -lpthread -llua -ldl -lm

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S), Darwin)
	SHARED = -fPIC -dynamiclib -Wl,-undefined,dynamic_lookup
else
	LDFLAGS += -ldl -lrt -Wl,-E
	SHARED = -fPIC --shared
endif

CPP_FILES = $(wildcard src/*.cpp)
OBJ_FILES = $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))

obj/%.o: src/%.cpp
	g++ -c $(CFLAGS) $< -o $@

all: $(OBJ_FILES)
