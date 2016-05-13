TARGET = iphone::9.2:9.0
ARCHS = armv7 arm64

include $(THEOS)/makefiles/common.mk

TOOL_NAME = rmaslr
rmaslr_FILES = main.cpp
rmaslr_PRIVATE_FRAMEWORKS = SpringBoardServices

rmaslr_CFLAGS = -w 

include $(THEOS_MAKE_PATH)/tool.mk
