include $(THEOS)/makefiles/common.mk
TARGET = iphone:9.2:9.0

TOOL_NAME = rmaslr
rmaslr_FILES = main.cpp
rmaslr_PRIVATE_FRAMEWORKS = SpringBoardServices
rmaslr_CFLAGS = -std=c++14

# rmaslr_LDFLAGS = -stdlib=libc++
# cannot add line above due to std::string linker errors

include $(THEOS_MAKE_PATH)/tool.mk
