CXX := g++
CXXFLAGS := -std=c++20
DEBUG_FLAGS := -g -Werror -Wall -Wextra
RELEASE_FLAGS := -O3

SRC := main.cpp
HDR := flagged_ptr.hpp cafe_heap.hpp Ordered_Circular_Queue.hpp
TARGET := program

DEBUG_DIR := debug
RELEASE_DIR := release

DEBUG_TARGET := $(DEBUG_DIR)/$(TARGET)
RELEASE_TARGET := $(RELEASE_DIR)/$(TARGET)

.PHONY: all debug release clean

all: debug release

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(DEBUG_TARGET)

release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(RELEASE_TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

$(DEBUG_DIR)/$(TARGET): $(SRC) $(HDR)
	@mkdir -p $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

$(RELEASE_DIR)/$(TARGET): $(SRC) $(HDR)
	@mkdir -p $(RELEASE_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -rf $(DEBUG_DIR) $(RELEASE_DIR)