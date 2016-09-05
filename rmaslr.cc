#include "rmaslr.h"

bool std::is_in_map(const std::vector<std::map<const char *, std::string>>& vector, const std::string& value) noexcept {
    for (const auto& item : vector) {
        for (const auto& item_ : item) {
            if (item_.second == value) {
                return true;
            }
        }
    }

    return false;
}

std::string std::find_last_component(const std::string& string) noexcept {
    auto pos = string.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string(string); //make a copy
    }

    if (pos == string.length()) {
        return std::string("");
    }

    return string.substr(pos + 1);
}

bool std::case_compare(const std::string& first, const std::string& second) noexcept {
    auto first_size = first.size();
    auto second_size = second.size();

    auto size = std::min(first_size, second_size);
    for (auto i = 0; i < size; i++) {
        char fc = tolower(first[i]);
        char sc = tolower(second[i]);

        if (fc == sc) {
            continue;
        }

        return fc < sc;
    }

    return first_size < second_size;
}

std::string rmaslr::platform::get_platform() noexcept {
    static std::string platform = load_from_filesystem();
    return platform;
}

std::string rmaslr::platform::load_from_filesystem() noexcept {
    const char *path = "/System/Library/CoreServices/SystemVersion.plist";
    if (access(path, F_OK) != 0) {
        error("platform::load_from_filesystem(); File does not exists at path (\"%s\"), possibly corrupted or not unix/linux system", path);
    }

    CFStringRef pathString = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
    if (!pathString) {
        error("platform::load_from_filesystem(); Unable to allocate CFStringRef, errno=%d(%s)", errno, strerror(errno));
    }

    CFURLRef pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false);
    if (!pathURL) {
        error("platform::load_from_filesystem(); Unable to allocate CFURLRef, errno=%d(%s)", errno, strerror(errno));
    }

    CFReadStreamRef pathStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, pathURL);
    if (!pathStream) {
        error("platform::load_from_filesystem(); Unable to create CFReadStream from file (\"%s\"), errno=%d(%s)", path, errno, strerror(errno));
    }

    CFReadStreamOpen(pathStream);

    CFErrorRef pathError = nullptr;
    CFDictionaryRef pathPlist = (CFDictionaryRef)CFPropertyListCreateWithStream(kCFAllocatorDefault, pathStream, 0, kCFPropertyListImmutable, nullptr, &pathError);

    if (pathError) {
        const char *error_string = CFStringGetCStringPtr(CFErrorCopyDescription(pathError), kCFStringEncodingUTF8);
        error("platform::load_from_filesystem(); Failed to open property list at path (\"%s\"), with error: \"%s\"", path, error_string);
    }

    if (!pathPlist) {
        error("platform::load_from_filesystem(); Failed to open property list at path (\"%s\"), errno=%d(%s)", path, errno, strerror(errno));
    }

    if (CFGetTypeID(pathPlist) != CFDictionaryGetTypeID()) {
        error("platform::load_from_filesystem(); Property list at path (\"%s\") is not a dictionary", path);
    }

    CFStringRef operatingSystemKey = CFStringCreateWithCString(kCFAllocatorDefault, "ProductName", kCFStringEncodingUTF8);
    if (!operatingSystemKey) {
        error("platform::load_from_filesystem(); Unable to allocate buffer for storing key (\"PlatformName\"), errno=%d(%s)", errno, strerror(errno));
    }

    if (!CFDictionaryContainsKey(pathPlist, operatingSystemKey)) {
        error("platform::load_from_filesystem(); Unable to find key (\"ProductName\") in dictionary", errno, strerror(errno));
    }

    CFStringRef platform = (CFStringRef)CFDictionaryGetValue(pathPlist, operatingSystemKey);
    if (!platform) {
        error("platform::load_from_filesystem(); Unable to get value for key (\"PlatformName\")", errno, strerror(errno));
    }

    if (CFGetTypeID(platform) != CFStringGetTypeID()) {
        error("platform::load_from_filesystem(); Platform from path (\"%s\") is not a string", path);
    }

    return CFStringGetCStringPtr(platform, kCFStringEncodingUTF8);
}

size_t rmaslr::get_size(size_t size) noexcept {
    size_t length = 1;
    if (!size) {
        return length;
    }

    double size_ = size;
    while (size_ / 10 >= 1) {
        length++;
        size_ /= 10;
    }

    return length;
}

uint32_t rmaslr::swap(uint32_t magic, uint32_t value) noexcept {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = ((value >> 8) & 0x00ff00ff) | ((value << 8) & 0xff00ff00);
        value = ((value >> 16) & 0x0000ffff) | ((value << 16) & 0xffff0000);
    }

    return value;
}

uint64_t rmaslr::swap(uint32_t magic, uint64_t value) noexcept {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = (value & 0x00000000ffffffff) << 32 | (value & 0xffffffff00000000) >> 32;
        value = (value & 0x0000ffff0000ffff) << 16 | (value & 0xffff0000ffff0000) >> 16;
        value = (value & 0x00ff00ff00ff0ff) << 8  | (value & 0xff00ff00ff00ff00) >> 8;
    }

    return value;
}

std::string rmaslr::formatted_string(const char *string, ...) noexcept {
    va_list list;

    va_start(list, string);
    int size = vsnprintf(nullptr, 0, string, list);
    va_end(list);

    std::string formatted(size + 1, '\0'); //+ 1 for null-byte

    va_start(list, string);
    vsprintf(const_cast<char *>(formatted.data()), string, list);
    va_end(list);

    return formatted;
}

std::map<const char *, std::string> rmaslr::parse_application_container(const std::string &path) noexcept {
    std::string name = std::find_last_component(path);

    auto information = std::map<const char *, std::string>();
    auto pos = name.find(".app");

    if (pos == std::string::npos) {
        return information;
    }

    if (pos == 0 || pos != (name.length() - (sizeof(".app") - 1))) {
        return information;
    }

    std::string infoPath = path + "/Contents/Info.plist";
    if (access(infoPath.c_str(), F_OK) != 0) {
        return information;
    }

    CFStringRef pathString = CFStringCreateWithCString(kCFAllocatorDefault, infoPath.c_str(), kCFStringEncodingUTF8);
    if (!pathString) {
        return information;
    }

    CFURLRef pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false);
    if (!pathString) {
        return information;
    }

    CFReadStreamRef pathStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, pathURL);
    if (!pathStream) {
        return information;
    }

    CFReadStreamOpen(pathStream);

    CFErrorRef pathError = nullptr;
    CFPropertyListRef pathPlist = CFPropertyListCreateWithStream(kCFAllocatorDefault, pathStream, 0, kCFPropertyListImmutable, nullptr, &pathError);

    if (pathError) {
        return information;
    }

    if (!pathPlist) {
        return information;
    }

    if (CFGetTypeID(pathPlist) != CFDictionaryGetTypeID()) {
        return information;
    }

    information = {
        { "bundleIdentifier", "" },
        { "containerName", name.substr(0, pos) },
        { "displayName", "" },
        { "executableName", "" },
        { "executablePath", ""}
    };

    CFStringRef applicationKey = CFStringCreateWithCString(kCFAllocatorDefault, "CFBundleName", kCFStringEncodingUTF8);
    CFStringRef identifierKey = CFStringCreateWithCString(kCFAllocatorDefault, "CFBundleIdentifier", kCFStringEncodingUTF8);
    CFStringRef executableKey = CFStringCreateWithCString(kCFAllocatorDefault, "CFBundleExecutable", kCFStringEncodingUTF8);

    CFDictionaryRef info = (CFDictionaryRef)pathPlist;
    if (CFDictionaryContainsKey(info, executableKey)) {
        CFStringRef executableName = (CFStringRef)CFDictionaryGetValue(info, executableKey);
        if (executableName) {
            if (CFGetTypeID(executableName) == CFStringGetTypeID()) {
                const char *executable_name = CFStringGetCStringPtr(executableName, kCFStringEncodingUTF8);
                information["executableName"] = executable_name;

                std::string executablePath = path + "/Contents/MacOS/";
                executablePath += executable_name;

                information["executablePath"] = executablePath;
            }
        }
    }

    if (CFDictionaryContainsKey(info, applicationKey)) {
        CFStringRef applicationName = (CFStringRef)CFDictionaryGetValue(info, applicationKey);
        if (applicationName) {
            if (CFGetTypeID(applicationName) == CFStringGetTypeID()) {
                information["displayName"] = CFStringGetCStringPtr(applicationName, kCFStringEncodingUTF8);
            }
        }
    }

    if (CFDictionaryContainsKey(info, identifierKey)) {
        CFStringRef identifier = (CFStringRef)CFDictionaryGetValue(info, identifierKey);
        if (identifier) {
            if (CFGetTypeID(identifier) == CFStringGetTypeID()) {
                information["bundleIdentifier"] = CFStringGetCStringPtr(identifier, kCFStringEncodingUTF8);
            }
        }
    }

    return information;
}

rmaslr::file::file(const char *path, const char *mode) : file_(fopen(path, mode)) {
    if (!file_) {
        error("rmaslr::file(); Unable to open file at path (\"%s\"), (fopen(path, mode) failed), errno=%d(%s)", path, errno, strerror(errno));
    }

    if (::stat(path, &sbuf_) != 0) {
        error("rmaslr::file(); Unable to gather information on file at path (\"%s\"), (::stat(path, &sbuf_) failed), errno=%d(%s)", path, errno, strerror(errno));
    }
}

rmaslr::file::file(FILE *file) : file_(file) {
    if (!file) {
        error("rmaslr::file() : file is null");
    }

    position_ = ftell(file_);
}

void rmaslr::file::seek(long position, rmaslr::file::seek_type seek) noexcept {
    switch (seek) {
    case seek_type::origin:
        if (sbuf_.st_size < position) {
            error("file::seek(); Cannot seek past end of file (position=%ld, size=%lld)", position, sbuf_.st_size);
        }

        position_ = position;
        break;
    case seek_type::current:
        if ((sbuf_.st_size - position_) < position) {
            error("file::seek(); Cannot seek past end of file (position=%ld, size=%lld)", position, sbuf_.st_size);
        }

        position_ += position;
        break;
    case seek_type::end:
        if (sbuf_.st_size < position) {
            error("file::seek(); Cannot seek past end of file (position=%ld, size=%lld)", position, sbuf_.st_size);
        }

        position_ = sbuf_.st_size - position;
        break;
    }

    if (fseek(file_, position, (int)seek) != 0) {
        error("file::seek(); Unable to seek to position (%ld), (fseek(file_, position, (int)seek) failed), errno=%d(%s)", position, errno, strerror(errno));
    }
}

bool rmaslr::options::application_ = false;
bool rmaslr::options::check_aslr_ = false;
bool rmaslr::options::display_archs_ = false;
