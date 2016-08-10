include $(THEOS)/makefiles/common.mk
TARGET = iphone:9.2:9.0

TOOL_NAME = rmaslr
rmaslr_FILES = main.cpp
rmaslr_PRIVATE_FRAMEWORKS = SpringBoardServices
rmaslr_CFLAGS = -std=c++14

# cannot add line below due to std::string linker errors
# rmaslr_LDFLAGS = -stdlib=libc++

include $(THEOS_MAKE_PATH)/tool.mk
