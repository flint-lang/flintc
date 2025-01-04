MAIN_FILE = src/main.cpp
TEST_FILE = tests/tests.cpp
SRC_FILES = $(filter-out $(MAIN_FILE), $(wildcard src/*.cpp))
HEADER_FILES = $(wildcard include/*.hpp) $(wildcard tests/*.hpp)

OBJ_DIR = build/obj
OUT_DIR = build/out

# Create object directory if it doesn't exist
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(OUT_DIR))

MAIN_OBJ = $(OBJ_DIR)/$(notdir $(MAIN_FILE:.cpp=.o))
TEST_OBJ = $(OBJ_DIR)/$(notdir $(TEST_FILE:.cpp=.o))
SRC_OBJS = $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))

CXX = clang++
STATIC_FLAGS = -static -L$(GLIBCPATH) -L$(ZLIBPATH) -Lxml2/xml2/lib

BASE_CXX_FLAGS = -g -O0 -Wall -Iinclude -Itests -std=c++17 -fno-omit-frame-pointer -DEBUG -funwind-tables -fstandalone-debug -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -I$(shell llvm-config --includedir)
BASE_LD_FLAGS = $(shell llvm-config --link-static --ldflags --system-libs --libs all)

TARGET = $(OUT_DIR)/flintc
TEST_TARGET = $(OUT_DIR)/tests

# The separator line (80 dashes)
define SEPARATOR
@printf "\n\e[97m"; for i in $$(seq 1 $$(tput cols)); do printf '─'; done; printf "\e[0m\n"
endef

.PHONY: clean all static dynamic

all: dynamic

dynamic: $(TARGET) $(TEST_TARGET)

static: CXX_FLAGS = $(BASE_CXX_FLAGS)
static: LD_FLAGS = $(BASE_LD_FLAGS) $(STATIC_FLAGS)
static: $(TARGET) $(TEST_TARGET)

# Rule for compiling source files
$(OBJ_DIR)/%.o: src/%.cpp $(HEADER_FILES)
	$(SEPARATOR)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

# Rule for compiling main file
$(MAIN_OBJ): $(MAIN_FILE) $(HEADER_FILES)
	$(SEPARATOR)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

# Rule for compiling test file
$(TEST_OBJ): $(TEST_FILE) $(HEADER_FILES)
	$(SEPARATOR)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

$(TARGET): $(MAIN_OBJ) $(SRC_OBJS)
	$(SEPARATOR)
	$(CXX) $(MAIN_OBJ) $(SRC_OBJS) -o $(TARGET) $(LD_FLAGS)

$(TEST_TARGET): $(TEST_OBJ) $(SRC_OBJS)
	$(SEPARATOR)
	$(CXX) $(TEST_OBJ) $(SRC_OBJS) -o $(TEST_TARGET) $(LD_FLAGS)

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) $(TEST_TARGET)
