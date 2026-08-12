TARGET := SpotifyDS
BUILD := build
SOURCES := source
INCLUDES := include

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -O2 -Wall -Wextra -Wno-attributes $(ARCH)
CXXFLAGS := $(CFLAGS) -std=gnu++17
LDFLAGS := -specs=3dsx.specs -g $(ARCH)
LIBS := -lctru -lm

include $(DEVKITPRO)/libctru/3ds_rules

.PHONY: all clean
