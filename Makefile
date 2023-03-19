
CC:=gcc
OUTPUT_CONTROLER :=  camera_capture

SRC_DIR_controler := ./src/c
OBJ_DIR_controler := build/c/obj
SRC_FILES_controler := $(wildcard $(SRC_DIR_controler)/*.c)
OBJ_FILES_controler := $(patsubst $(SRC_DIR_controler)/%.c,$(OBJ_DIR_controler)/%.o,$(SRC_FILES_controler))


# CXXFLAGS := -O3 -Wall -Wextra  -fsanitize=address
CXXFLAGS := -Wall -Wextra  -fsanitize=address -lpthread


$(OBJ_DIR_controler)/%.o: $(SRC_DIR_controler)/%.c
	mkdir -p $(OBJ_DIR_controler)
	$(CC)   $(CXXFLAGS)   -c -o $@ $^

$(OUTPUT_CONTROLER): $(OBJ_FILES_controler)
	$(CC)  -o $@ $^   $(CXXFLAGS)



all: $(OUTPUT_CONTROLER)


clean :
	rm -rf build
	rm $(OUTPUT_CONTROLER)

