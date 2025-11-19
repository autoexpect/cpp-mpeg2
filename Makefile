CXX ?= g++
CC ?= gcc
AR ?= ar

CXXFLAGS = -std=c++11 -Iinclude -Wall -g
LDFLAGS = 

SRCS = src/bitstream.cpp src/utils.cpp src/ts_muxer.cpp src/ps_muxer.cpp src/codec_utils.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET_LIB = libmpeg2.a
TARGET_TS_TEST = ts_test
TARGET_PS_TEST = ps_test

all: $(TARGET_LIB) $(TARGET_TS_TEST) $(TARGET_PS_TEST)

$(TARGET_LIB): $(OBJS)
	$(AR) rcs $@ $^

$(TARGET_TS_TEST): examples/ts_test.o $(TARGET_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_PS_TEST): examples/ps_test.o $(TARGET_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) examples/*.o $(TARGET_LIB) $(TARGET_TS_TEST) $(TARGET_PS_TEST)

.PHONY: all clean
