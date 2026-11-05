CXX = g++
CXXFLAGS = -std=c++17 -O3 -mavx2 -fopenmp -Wall -Wextra -march=native
INCLUDES = -Iinclude

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Removed engine, stream_loader, and telemetry.cpp (now handled directly in main.cpp)
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/baselines.cpp \
       $(SRC_DIR)/simd_detector.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/fraud_engine.exe

.PHONY: all clean run setup_dirs

all: setup_dirs $(TARGET)

setup_dirs:
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"
	@if not exist "data" mkdir "data"

$(TARGET): $(OBJS)
	@echo Linking binary: $@
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo Compiling: $<
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: all
	@echo Executing fraud detection engine...
	$(TARGET)

clean:
	@echo Cleaning build artifacts...
	@if exist "$(OBJ_DIR)" rmdir /s /q "$(OBJ_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"