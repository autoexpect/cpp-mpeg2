CXX ?= g++
AR ?= ar

# CXXFLAGS is yours to override (e.g. make CXXFLAGS="-O1 -g -fsanitize=address");
# the flags the build cannot do without are kept separate so they always apply.
CXXFLAGS ?= -O2 -g
BUILD_FLAGS = -std=c++11 -Iinclude -Wall -Wextra -MMD -MP
ALL_CXXFLAGS = $(BUILD_FLAGS) $(CPPFLAGS) $(CXXFLAGS)
LDFLAGS ?=

SRCS = src/bitstream.cpp src/ts_muxer.cpp src/codec_utils.cpp src/aac_utils.cpp
OBJS = $(SRCS:.cpp=.o)
EXAMPLE_OBJS = examples/ts_test.o
DEPS = $(OBJS:.o=.d) $(EXAMPLE_OBJS:.o=.d)

TARGET_LIB = libmpeg2.a
TARGET_TS_TEST = ts_test

all: $(TARGET_LIB) $(TARGET_TS_TEST)

$(TARGET_LIB): $(OBJS)
	$(AR) rcs $@ $^

$(TARGET_TS_TEST): $(EXAMPLE_OBJS) $(TARGET_LIB)
	$(CXX) $(ALL_CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXAMPLE_OBJS) $(DEPS) $(TARGET_LIB) $(TARGET_TS_TEST)

-include $(DEPS)

.PHONY: all clean
