MAIN_FILE = src/main.cpp
TEST_FILE = src/tests.cpp
SRC_FILES = src/error.cpp src/lexer.cpp src/linker.cpp src/parser.cpp src/signature.cpp src/debug.cpp src/generator.cpp
HEADER_FILES = $(shell find ./include -name '*.hpp')

OBJ_DIR = build/obj
OUT_DIR = build/out
# Create object directory if it doesn't exist
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(OUT_DIR))

MAIN_OBJ = $(OBJ_DIR)/$(notdir $(MAIN_FILE:.cpp=.o))
TEST_OBJ = $(OBJ_DIR)/$(notdir $(TEST_FILE:.cpp=.o))
SRC_OBJS = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

CXX = clang++
CXX_FLAGS = -g -O0 -Wall -Iinclude -std=c++17 -funwind-tables -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -I$(shell llvm-config --includedir)
LD_FLAGS = $(shell llvm-config --link-static --ldflags --system-libs --libs core)

TARGET = $(OUT_DIR)/flintc

.PHONY: clean all

all: $(TARGET)

$(OBJ_DIR)/%.o: src/%.cpp $(HEADER_FILES)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

$(TARGET): $(MAIN_OBJ) $(SRC_OBJS)
	$(CXX) $(MAIN_OBJ) $(SRC_OBJS) -o $(TARGET) $(LD_FLAGS)

clean:
	rm $(OBJ_DIR)/*.o
