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

SRCS = $(wildcard src/*.cpp)

all: $(SRCS)
	g++ -c $(CFLAGS) $< -o $@
