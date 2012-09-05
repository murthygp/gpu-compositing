.PHONY: all install uninstall clean

-include rules.make

INCS     ?= -I$(GSDK_ROOT)/include/OGLES2 \
            -I$(GSDK_ROOT)/include/bufferclass_ti \
            -I$(CMEM_DIR)/include 

LIB_PATH ?= -L$(GSDK_ROOT)/gfx_rel_es6.x \
            -Wl,--rpath=$(GSDK_ROOT)/gfx_rel_es6.x

INST_DEST    ?= $(TGTFS_PATH)/opt/gpu-compositing

CFLAGS   := -W -Wall -O2 -DLINUX $(INCS)
LIBS     := -lEGL
LDFLAGS  := $(LIB_PATH)

LIBS    += -lGLESv2
CMEM_LIB     ?= $(CMEM_DIR)/lib/cmem.a470MV
LIBS    += $(CMEM_LIB)

TARGET = composition 

SOURCES = main.c common.c
HEADERS = common.h
OBJFILES = $(SOURCES:%.c=%.o)

all:	$(TARGET)

$(TARGET):	$(OBJFILES)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(OBJFILES):	%.o: %.c $(HEADERS)
	$(CC) -c $< -o $@ $(CFLAGS)

install:	$(TARGET)
	mkdir -p $(INST_DEST)
	install -m 0755 $^ $(INST_DEST)
	cp ../targetfs/init.sh $(INST_DEST) 

uninstall:
	cd $(INST_DEST) && rm -f $(TARGET)

.PHONY: clean
clean:
	-rm -f $(OBJFILES) $(TARGET)
