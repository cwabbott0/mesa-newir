CUR_DIR = $(shell pwd)
TOP_SRC_DIR = $(CUR_DIR)

DEBUG_FLAGS ?= -g -O0

CFLAGS += -I $(TOP_SRC_DIR) -Wall -fPIC --std=c99 $(DEBUG_FLAGS)
CXXFLAGS += -I $(TOP_SRC_DIR) -Wall -fPIC $(DEBUG_FLAGS)
LDFLAGS += -lm $(DEBUG_FLAGS)

SOURCE_DIRS = . main program

C_OBJECTS = $(patsubst %.c, %.o, $(foreach dir, $(SOURCE_DIRS), $(wildcard $(dir)/*.c)))
CXX_OBJECTS = $(patsubst %.cpp, %.o, $(foreach dir, $(SOURCE_DIRS), $(wildcard $(dir)/*.cpp)))

NAME = nir_test

all: $(NAME)

clean:
	rm -f $(LIB_NAME)
	rm -f $(C_OBJECTS)
	rm -f $(CXX_OBJECTS)

$(NAME): $(C_OBJECTS) $(CXX_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(C_OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(CXX_OBJECTS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all clean