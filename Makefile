
CC:=gcc
OUTPUT_CONTROLER :=  camera_capture

SRC_DIR_controler := ./src/c
OBJ_DIR_controler := build/c/obj
SRC_FILES_controler := $(wildcard $(SRC_DIR_controler)/*.c)
OBJ_FILES_controler := $(patsubst $(SRC_DIR_controler)/%.c,$(OBJ_DIR_controler)/%.o,$(SRC_FILES_controler))


# CXXFLAGS := -O3 -Wall -Wextra  -fsanitize=address
CXXFLAGS := -Wall -O3 -Wextra  -fsanitize=address -lpthread

LIBS:=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gstreamer-1.0 --libs)
LIBS:=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gstreamer-video-1.0 --libs)
LIBS+=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gobject-introspection-1.0 --libs)
LIBS+=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config tcam --libs)

# to build within the source tree
# enable the following lines
# and source build directory ./env.sh
# LIBS+=-L./../../build/libs

CFLAGS:=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gstreamer-1.0 --cflags)
CFLAGS:=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gstreamer-video-1.0 --cflags)
CFLAGS+=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config gobject-introspection-1.0 --cflags)
CFLAGS+=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config tcam --cflags)

$(OBJ_DIR_controler)/%.o: $(SRC_DIR_controler)/%.c
	mkdir -p $(OBJ_DIR_controler)
	$(CC) -ltcam-property   $(CXXFLAGS)  $(CFLAGS)   -c -o $@ $^ $(LIBS)

$(OUTPUT_CONTROLER): $(OBJ_FILES_controler)
	$(CC) -ltcam-property  $(CFLAGS)   -o $@ $^   $(CXXFLAGS) $(LIBS)


all: $(OUTPUT_CONTROLER)


clean :
	rm -rf build
	rm $(OUTPUT_CONTROLER)

