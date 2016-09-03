//
//  main.cpp
//  rmaslr
//
//  Created by iNoahDev on 5/10/16.
//  Copyright © 2016 iNoahDev. All rights reserved.
//

#include <cstddef>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>

#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#define assert_(str, ...) fprintf(stderr, "\x1B[31mError:\x1B[0m " str "\n", ##__VA_ARGS__); return -1

#define notice(str, ...) fprintf(stdout, "\x1B[33mNotice:\x1B[0m " str "\n", ##__VA_ARGS__);
#define error(str, ...) fprintf(stderr, "\x1B[31mError:\x1B[0m " str "\n", ##__VA_ARGS__); exit(0)

//compatibility with linter-clang and older headers
#if !defined(FAT_MAGIC_64) && !defined(FAT_CIGAM_64)
struct fat_arch_64 {
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    uint64_t offset;
    uint64_t size;
    uint32_t align;
};

#define FAT_MAGIC_64 0xcafebabf
#define FAT_CIGAM_64 0xbfbafeca
#endif

#ifdef DEBUG
#define log(str, ...) fprintf(stderr, "[rmaslr] " __FILE__ ":%d DEBUG: " str "\n", __LINE__, ##__VA_ARGS__);
#else
#define log(str, ...)
#endif

static uint32_t swap(uint32_t magic, uint32_t value) noexcept {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = ((value >> 8) & 0x00ff00ff) | ((value << 8) & 0xff00ff00);
        value = ((value >> 16) & 0x0000ffff) | ((value << 16) & 0xffff0000);
    }

    return value;
}

static uint64_t swap(uint32_t magic, uint64_t value) noexcept {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = (value & 0x00000000ffffffff) << 32 | (value & 0xffffffff00000000) >> 32;
        value = (value & 0x0000ffff0000ffff) << 16 | (value & 0xffff0000ffff0000) >> 16;
        value = (value & 0x00ff00ff00ff0ff) << 8  | (value & 0xff00ff00ff00ff00) >> 8;
    }

    return value;
}

static inline int32_t swap(uint32_t magic, int32_t value) noexcept {
    return static_cast<int32_t>(swap(magic, static_cast<uint32_t>(value)));
}

__attribute__((unused)) static inline int64_t swap(uint32_t magic, int64_t value) noexcept {
    return static_cast<int64_t>(swap(magic, static_cast<uint64_t>(value)));
}

static bool is_ios = false;

namespace std {
    auto is_in_map = [](std::vector<std::map<const char *, std::string>> vector, std::string value) noexcept {
        for (const auto& item : vector) {
            for (const auto& item_ : item) {
                if (item_.second == value) {
                    return true;
                }
            }
        }

        return false;
    };

    auto find_last_component = [](const std::string& string) noexcept {
        auto pos = string.find_last_of('/');
        if (pos == std::string::npos || pos == string.length()) {
            //must specify std::string since compiler thinks we're using const char*
            return std::string("");
        }

        return string.substr(pos + 1);
    };

    auto print_vector = [](std::vector<std::map<const char *, std::string>>& vector, bool use_listing = false) noexcept {
        auto sorted_vector = std::vector<std::map<const char *, std::string>>();
        std::string first_name;

        for (auto& information : vector) {
            auto information_ = information;

            std::string name = information_["containerName"];
            if (name.empty() || std::is_in_map(sorted_vector, name)) {
                name = information_["displayName"];
                if (name.empty() || std::is_in_map(sorted_vector, name)) {
                    name = information_["executableName"];
                    if (!use_listing) {
                        information_["displayName"] = "";
                        information_["containerName"] = "";
                    }
                } else if (!use_listing) {
                    information_["executableName"] = "";
                    information_["containerName"] = "";
                }
            } else if (!use_listing) {
                information_["displayName"] = "";
                information_["executableName"] = "";
            }

            sorted_vector.push_back(information);
        }

        std::sort(sorted_vector.begin(), sorted_vector.end(), [](std::map<const char *, std::string>& first, std::map<const char *, std::string>& second) {
            std::string first_name = first["containerName"];
            if (first_name.empty()) {
                first_name = first["displayName"];
                if (first_name.empty()) {
                    first_name = first["executableName"];
                    if (first_name.empty()) {
                        return false;
                    }
                }
            }

            std::string second_name = second["containerName"];
            if (second_name.empty()) {
                second_name = second["displayName"];
                if (second_name.empty()) {
                    second_name = second["executableName"];
                    if (second_name.empty()) {
                        return false;
                    }
                }
            }

            auto length = std::min(first_name.length(), second_name.length());
            for (auto i = 0; i < length; i++) {
                char fc = tolower(first_name[i]);
                char sc = tolower(second_name[i]);

                if (fc == sc) {
                    continue;
                }

                return fc < sc;
            }

            fprintf(stdout, "called here!\"%s\" - \"%s\"\n", first_name.c_str(), second_name.c_str());
            return first_name.size() < second_name.size();
        });

        if (use_listing) {
            auto max_container_size = 0;
            auto max_display_size = 0;
            auto max_executable_name_size = 0;

            for (auto& information : sorted_vector) {
                if (!is_ios) {
                    auto container_length = information["containerName"].length();
                    if (max_container_size < container_length) {
                        max_container_size = container_length;
                    }
                }

                auto display_length = information["displayName"].length();
                if (max_display_size < display_length) {
                    max_display_size = display_length;
                }

                auto executable_length = information["executableName"].length();
                if (max_executable_name_size < executable_length) {
                    max_executable_name_size = executable_length;
                }
            }

            auto get_spaces = [](std::string::size_type length) {
                std::string return_value;
                return_value.resize(length, ' ');

                return return_value;
            };

            std::string container_spaces;
            if (!is_ios) {
                container_spaces = get_spaces(max_container_size + 1);
            }

            std::string display_spaces = get_spaces(max_display_size + 1);
            std::string executable_spaces = get_spaces(max_executable_name_size + 1);

            auto get_size = [](size_t size) {
                auto length = 0;
                double size_ = size;

                while (size_ / 10 >= 1) {
                    size_ /= 10;
                    length++;
                }

                return length++;
            };

            auto i = 0;
            auto size_spaces = get_spaces(get_size(sorted_vector.size()));

            for (auto& information : sorted_vector) {
                if (is_ios) {
                    fprintf(stdout, "%d. %sApplication (Display Name: \"%s\",%sExecutable Name: \"%s\",%sBundle Identifier: \"%s\")\n", i, &size_spaces[get_size(i)], information["displayName"].c_str(), &display_spaces[information["displayName"].length()], information["executableName"].c_str(), &executable_spaces[information["executableName"].length()], information["bundleIdentifier"].c_str());
                } else {
                    fprintf(stdout, "%d. %sApplication (Container Name: \"%s\",%sDisplay Name: \"%s\",%sExecutable Name: \"%s\",%sBundle Identifier: \"%s\")\n", i, &size_spaces[get_size(i)], information["containerName"].c_str(), &container_spaces[information["containerName"].length()], information["displayName"].c_str(), &display_spaces[information["displayName"].length()], information["executableName"].c_str(), &executable_spaces[information["executableName"].length()], information["bundleIdentifier"].c_str());
                }

                i++;
            }
        } else {
            auto front = sorted_vector.front();

            std::string name = front["containerName"];
            if (name.empty()) {
                name = front["displayName"];
                if (name.empty()) {
                    name = front["executableName"];
                }
            }

            fprintf(stdout, "%s", name.c_str());
            sorted_vector.erase(sorted_vector.begin());

            for (auto& information : sorted_vector) {
                name = information["containerName"];
                if (name.empty()) {
                    name = information["displayName"];
                    if (name.empty()) {
                        name = information["executableName"];
                        if (name.empty()) {
                            continue;
                        }
                    }
                }

                fprintf(stdout, ", %s", name.c_str());
            }

            fprintf(stdout, "\n");
        }
    };
}

namespace Environment {
    auto GetCurrentDirectory = []() noexcept {
        size_t size = PATH_MAX;
        char *buf = new char[size];

        if (!buf) {
            error("Unable to allocate buffer (size=%ld) to get current working directory, errno=%d (%s)", size, errno, strerror(errno));
        }

        char *result = getcwd(buf, size);
        if (!result) {
            if (errno == ERANGE) {
                do {
                    size += 8;

                    buf = new char[size];
                    if (!buf) {
                        error("Unable to allocate buffer (size=%ld) to get current working directory, errno=%d (%s)", size, errno, strerror(errno));
                    }

                    result = getcwd(buf, size);
                } while (!result && errno == ERANGE);
            } else {
                error("Unable to get the current working directory, errno=%d (%s)", errno, strerror(errno));
            }
        }

        std::string buffer = result;
        if (buffer.back() != '/') {
            buffer.append(1, '/');
        }

        return buffer;
    };

    static std::string CurrentDirectory = GetCurrentDirectory();
}

static CFArrayRef (*SBSCopyApplicationDisplayIdentifiers)(bool onlyActive, bool debugging) = nullptr;
static CFStringRef (*SBSCopyLocalizedApplicationNameForDisplayIdentifier)(CFStringRef bundle_id) = nullptr;
static CFStringRef (*SBSCopyExecutablePathForDisplayIdentifier)(CFStringRef bundle_id) = nullptr;

void print_usage() noexcept {
    fprintf(stdout, "Usage: rmaslr -a application\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "    -a,     --app/--application,   Remove ASLR for an application\n");
    fprintf(stdout, "    -apps,  --applications,        Print a list of Applications\n");
    fprintf(stdout, "    -arch,  --architecture,        Single out an architecture to remove ASLR from\n");
    fprintf(stdout, "    -archs, --architectures,       Print all possible architectures, and if application/binary is provided, print all architectures present\n");
    fprintf(stdout, "    -b,     --binary,              Remove ASLR for a Mach-O Executable\n");
    fprintf(stdout, "    -c,     --check,               Check if application or binary contains ASLR\n");
    fprintf(stdout, "    -h,     --help,                Print this message\n");
    fprintf(stdout, "    -u,     --usage,               Print this message\n");

    exit(0);
}

int main(int argc, const char * argv[], const char * envp[]) noexcept {
    if (argc < 2) {
        print_usage();
    }

    //for fancy debug messages
    bool uses_application = false;

    bool display_archs_only = false;
    bool check_aslr_only = false;

    const char *name = nullptr;
    const char *binary_path = nullptr;

    //needs a better method
    is_ios = access("/System/Library/PrivateFrameworks/SpringBoardServices.framework", F_OK) == 0;
    void *handle = nullptr;

    if (is_ios) {
        handle = dlopen("/System/Library/PrivateFrameworks/SpringBoardServices.framework/SpringBoardServices", RTLD_NOW);
        if (!handle) {
            assert_("Unable to load Required Framework: SpringBoardServices");
        }

        SBSCopyApplicationDisplayIdentifiers = (CFArrayRef(*)(bool, bool))dlsym(handle, "SBSCopyApplicationDisplayIdentifiers");
        SBSCopyLocalizedApplicationNameForDisplayIdentifier = (CFStringRef (*)(CFStringRef))dlsym(handle, "SBSCopyLocalizedApplicationNameForDisplayIdentifier");
        SBSCopyExecutablePathForDisplayIdentifier = (CFStringRef (*)(CFStringRef))dlsym(handle, "SBSCopyExecutablePathForDisplayIdentifier");

        if (!SBSCopyApplicationDisplayIdentifiers || !SBSCopyLocalizedApplicationNameForDisplayIdentifier || !SBSCopyExecutablePathForDisplayIdentifier) {
            assert_("Unable to load required functions from Required Framework: SpringBoardServices");
        }
    }

    auto default_architectures = std::vector<const NXArchInfo *>();
    auto default_architectures_original_size = 0;

    const char *argument = argv[1];
    if (argument[0] != '-') {
        assert_("%s is not an option", argument);
    }

    const char *option = &argument[1];
    if (option[0] == '-') {
        option++;
    }

    auto parse_application_container = [](const std::string& path) {
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
    };

    auto find_last_component = [](const char *string) {
        char const *result = string;
        char const *result_ = string;

        while ((result = strchr(result_, '/'))) {
            result_ = result;
            result_++;
        }

        return &string[(uintptr_t)result_ - (uintptr_t)string];
    };

    if (strcmp(option, "h") == 0 || strcmp(option, "help") == 0 || strcmp(option, "u") == 0 || strcmp(option, "usage") == 0) {
        if (argc > 2) {
            assert_("Please run %s seperately", argument);
        }

        print_usage();
    } else if (strcmp(option, "apps") == 0 || strcmp(option, "-applications") == 0) {
        bool use_listing = false;
        if (argc > 2) {
            if (argc > 3) {
                assert_("Too many arguments provided");
            }

            option = argv[2];
            if (strcmp(option, "-list") == 0 || strcmp(option, "--list") == 0) {
                use_listing = true;
            } else {
                assert_("Unrecognized argument: \"%s\"", option);
            }
        }

        auto applications = std::vector<std::map<const char *, std::string>>();

        if (is_ios) {
            CFArrayRef applications_ = SBSCopyApplicationDisplayIdentifiers(false, false);
            if (!applications_) {
                assert_("Unable to retrieve application-list");
            }

            CFIndex applications_count = CFArrayGetCount(applications_);
            for (CFIndex i = 0; i < applications_count; i++) {
                CFStringRef bundle_id = (CFStringRef)CFArrayGetValueAtIndex(applications_, i);

                const char *displayName = CFStringGetCStringPtr(SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundle_id), kCFStringEncodingUTF8);
                const char *bundleIdentifier = "";

                const char *executablePath = "";
                const char *executableName = "";

                if (use_listing) {
                    bundleIdentifier = CFStringGetCStringPtr(bundle_id, kCFStringEncodingUTF8);
                    executablePath = CFStringGetCStringPtr(SBSCopyExecutablePathForDisplayIdentifier(bundle_id), kCFStringEncodingUTF8);

                    executableName = find_last_component(executablePath);
                }

                if (!displayName || !bundleIdentifier || !executablePath || !executableName) {
                    continue;
                }

                applications.push_back({{ "bundleIdentifier", bundleIdentifier }, { "displayName", displayName }, { "containerName", "" }, { "executableName", executableName }, { "executablePath", executablePath }});
            }
        } else {
            DIR *directory = opendir("/Applications");
            if (!directory) {
                error("Unable to access directory \"/Applications\".");
            }

            std::string applicationDirectory = "/Applications/";
            struct dirent *dir_entry = nullptr;

            while ((dir_entry = readdir(directory))) {
                auto application = applicationDirectory + dir_entry->d_name;
                auto application_information = parse_application_container(application);

                if (application_information.empty()) {
                    continue;
                }

                applications.push_back(application_information);
            }
        }

        std::print_vector(applications, use_listing);
        return 0;
    } else if (strcmp(option, "archs") == 0) {
        if (argc > 2) {
            assert_("Please run %s seperately", argument);
        }

        //to sort alphabetically
        auto arch_names = std::vector<const char *>();

        const NXArchInfo *archInfos = NXGetAllArchInfos();
        while (archInfos && archInfos->name) {
            arch_names.push_back(archInfos->name);
            archInfos++; //retrieve next "const NXArchInfo *" object
        }

        std::sort(arch_names.begin(), arch_names.end(), [](const char *string1, const char *string2) {
            return strcmp(string1, string2) < 0;
        });

        for (auto const& arch : arch_names) {
            fprintf(stdout, "%s\n", arch);
        }

        return 0;
    }

    for (int i = 1; i < argc; i++) {
        argument = argv[i];
        if (argument[0] != '-') {
            assert_("%s is not an option", argument);
        }

        option = &argument[1];
        if (option[0] == '-') {
            option++;
        }

        bool last_argument = i == argc - 1;

        if (strcmp(option, "a") == 0 || strcmp(option, "app") == 0 || strcmp(option, "application") == 0) {
            if (last_argument) {
                assert_("Please provide an application display-name/identifier/executable-name");
            }

            i++;

            auto applications_found = std::vector<std::map<const char *, std::string>>();
            auto app_name = argv[i];

            if (is_ios) {
                CFArrayRef apps = SBSCopyApplicationDisplayIdentifiers(false, false);
                if (!apps) {
                    assert_("Unable to retrieve application-list");
                }

                CFIndex count = CFArrayGetCount(apps);
                for (CFIndex i = 0; i < count; i++) {
                    if (binary_path) {
                        binary_path = nullptr;
                    }

                    CFStringRef bundle_id = (CFStringRef)CFArrayGetValueAtIndex(apps, i);
                    if (!bundle_id) {
                        continue;
                    }

                    char const * display_name = CFStringGetCStringPtr(SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundle_id), kCFStringEncodingUTF8);
                    char const * executable_path_ = CFStringGetCStringPtr(SBSCopyExecutablePathForDisplayIdentifier(bundle_id), kCFStringEncodingUTF8);
                    char const * bundle_id_ = CFStringGetCStringPtr(bundle_id, kCFStringEncodingUTF8);

                    char const * executable_name = find_last_component(executable_path_);

                    //apparently "iTunesU" has a null display name?
                    if (!display_name || !executable_path_ || !bundle_id_) {
                        continue;
                    }

                    const char *name_ = display_name;
                    if (!strlen(name_) || strcmp(name_, app_name) != 0) {
                        name_ = bundle_id_;
                        if (!strlen(name_) || strcmp(name_, app_name) != 0) {
                            name_ = executable_name;
                            if (!strlen(name_) || strcmp(name_, app_name) != 0) {
                                continue;
                            }
                        }
                    }

                    auto information = std::map<const char *, std::string>();
                    information = {
                        { "bundleIdentifier", bundle_id_ },
                        { "containerName", "" },
                        { "displayName", display_name },
                        { "executableName", executable_name },
                        { "executablePath", executable_path_ }
                    };

                    applications_found.push_back(information);
                }
            } else {
                DIR *directory = opendir("/Applications");
                if (!directory) {
                    error("Unable to access directory \"/Applications\".");
                }

                auto applicationDirectory = std::string("/Applications/");
                struct dirent *dir_entry = nullptr;

                while ((dir_entry = readdir(directory))) {
                    auto application = applicationDirectory + dir_entry->d_name;
                    auto information = parse_application_container(application);

                    if (information.empty()) {
                        continue;
                    }

                    std::string name = information["containerName"];
                    if (name.empty() || name != app_name) {
                        name = information["displayName"];
                        if (name.empty() || name != app_name) {
                            name = information["bundleIdentifier"];
                            if (name.empty() || name != app_name) {
                                name = information["executableName"];
                                if (name.empty() || name != app_name) {
                                    continue;
                                }
                            }
                        }
                    }

                    applications_found.push_back(information);
                }
            }

            if (applications_found.empty()) {
                assert_("Unable to find application \"%s\"", app_name);
            }

            if (applications_found.size() > 1) {
                fprintf(stdout, "Multiple Applications with name (\"%s\") have been found:\n", app_name);

                auto max_container_size = 0;
                auto max_display_size = 0;
                auto max_executable_name_size = 0;

                for (auto& information : applications_found) {
                    if (!is_ios) {
                        auto container_length = information["containerName"].length();
                        if (max_container_size < container_length) {
                            max_container_size = container_length;
                        }
                    }

                    auto display_length = information["displayName"].length();
                    if (max_display_size < display_length) {
                        max_display_size = display_length;
                    }

                    auto executable_length = information["executableName"].length();
                    if (max_executable_name_size < executable_length) {
                        max_executable_name_size = executable_length;
                    }
                }

                auto get_spaces = [](std::string::size_type length) {
                    std::string return_value;
                    return_value.resize(length, ' ');

                    return return_value;
                };

                std::string container_spaces;
                if (!is_ios) {
                    container_spaces = get_spaces(max_container_size + 1);
                }

                std::string display_spaces = get_spaces(max_display_size + 1);
                std::string executable_spaces = get_spaces(max_executable_name_size + 1);

                auto get_size = [](size_t size) {
                    auto length = 0;
                    double size_ = size;

                    while (size_ / 10 >= 1) {
                        size_ /= 10;
                        length++;
                    }

                    return length++;
                };

                auto i = 0;
                auto size_spaces = get_spaces(get_size(applications_found.size()));

                for (auto iter = applications_found.begin(); iter != applications_found.end(); iter++) {
                    auto information = *iter;

                    if (is_ios) {
                        fprintf(stdout, "%d. %sApplication (Display Name: \"%s\",%sExecutable Name: \"%s\",%sBundle Identifier: \"%s\")\n", i, &size_spaces[get_size(i)], information["displayName"].c_str(), &display_spaces[information["displayName"].length()], information["executableName"].c_str(), &executable_spaces[information["executableName"].length()], information["bundleIdentifier"].c_str());
                    } else {
                        fprintf(stdout, "%d. %sApplication (Container Name: \"%s\",%sDisplay Name: \"%s\",%sExecutable Name: \"%s\",%sBundle Identifier: \"%s\")\n", i, &size_spaces[get_size(i)], information["containerName"].c_str(), &container_spaces[information["containerName"].length()], information["displayName"].c_str(), &display_spaces[information["displayName"].length()], information["executableName"].c_str(), &executable_spaces[information["executableName"].length()], information["bundleIdentifier"].c_str());
                    }

                    i++;
                }

                int result = -1;
                while (result < 1 || result > applications_found.size()) {
                    std::cout << "Please select one of the applications by number: ";
                    std::cin >> result;
                }

                auto application_information = applications_found[result - 1];
                binary_path = strdup(application_information["executablePath"].c_str());

                auto displayName = application_information["displayName"];
                auto containerName = application_information["containerName"];
                auto executableName = application_information["executableName"];

                auto iter = std::find(applications_found.begin(), applications_found.end(), application_information);
                if (iter != applications_found.end()) {
                    applications_found.erase(iter);
                }

                if (!std::is_in_map(applications_found, containerName) && !is_ios) {
                    name = strdup(containerName.c_str());
                } else if (!std::is_in_map(applications_found, displayName)) {
                    name = strdup(displayName.c_str());
                } else if (!std::is_in_map(applications_found, executableName)) {
                    name = strdup(executableName.c_str());
                } else {
                    name = app_name;
                }
            } else {
                auto application_information = applications_found.front();

                if (is_ios) {
                    name = strdup(application_information["displayName"].c_str());
                } else {
                    name = app_name;
                }

                binary_path = strdup(application_information["executablePath"].c_str());
            }

            uses_application = true;
            if (access(binary_path, R_OK) != 0) {
                if (access(binary_path, F_OK) != 0) {
                    assert_("Application (%s)'s executable does not exist, at path (%s)", name, binary_path);
                }

                assert_("Unable to read Application (%s)'s executable", name);
            }
        } else if (strcmp(option, "b") == 0 || strcmp(option, "binary") == 0) {
            if (last_argument) {
                assert_("Please provide a path to a mach-o binary");
            }

            i++;
            const char *path = argv[i];

            if (path[0] != '/') {
                std::string current_directory = Environment::CurrentDirectory;

                current_directory.append(path);
                path = current_directory.c_str();
            }

            struct stat sbuf;
            if (stat(path, &sbuf) != 0) {
                assert_("Unable to get information on file at path (%s)", path);
            }

            name = find_last_component(path);

            if (!is_ios && S_ISDIR(sbuf.st_mode)) {
                auto path_ = std::string(path);
                auto information = parse_application_container(path_);

                if (information.empty()) {
                    assert_("Directory at path (%s) is not an application", path);
                }

                path = information["executablePath"].c_str();
                if (!strlen(path) || access(path, F_OK) != 0) {
                    assert_("Executable at path (\"%s\") is not valid (either not found in Info.plist or not present on the filesystem)", path);
                }

                uses_application = true;
            }

            binary_path = path;
            if (access(binary_path, R_OK) != 0) {
                if (access(binary_path, F_OK) != 0) {
                    assert_("File at path (%s) does not exist", binary_path);
                }

                assert_("Unable to read file at path (%s)", binary_path);
            }
        } else if (strcmp(option, "arch") == 0 || strcmp(option, "architecture") == 0) {
            if (last_argument) {
                assert_("Please provide an architecture name");
            }

            if (!binary_path) {
                assert_("Please select an application or binary first");
            }

            if (display_archs_only) {
                assert_("Cannot print architectures and choose an architecture from which to remove ASLR from at the same time");
            }

            if (check_aslr_only) {
                assert_("Cannot check aslr status and select an architecture from which to remove ASLR from at the same time");
            }

            int j = ++i;

            for (; j < argc; j++) {
                const char *architecture = argv[j];
                const NXArchInfo *archInfo = NXGetArchInfoFromName(architecture);

                if (!archInfo) {
                    break;
                }

                default_architectures.push_back(archInfo);
            }

            default_architectures_original_size = default_architectures.size();
            if (!default_architectures_original_size) {
                assert_("%s is not a valid architecture", argv[j]);
            }

            i = j;
        } else if (strcmp(option, "archs") == 0 || strcmp(option, "architectures") == 0) {
            if (!binary_path) {
                assert_("Please select an application or binary first");
            }

            if (check_aslr_only) {
                assert_("Cannot both display architectures and display aslr-status");
            }

            if (default_architectures.size()) {
                assert_("Cannot both display architectures and select an arch to remove ASLR from");
            }

            if (display_archs_only) {
                assert_("rmaslr is already configured to only print architectures");
            }

            display_archs_only = true;
        } else if (strcmp(option, "c") == 0 || strcmp(option, "check") == 0) {
            if (!binary_path) {
                assert_("Please select an application or binary first");
            }

            if (display_archs_only) {
                assert_("Can't display architectures & aslr-status at the same time");
            }

            if (check_aslr_only) {
                assert_("rmaslr is already configured to check for aslr");
            }

            check_aslr_only = true;
        } else {
            assert_("Unrecognized option %s", argument);
        }
    }

    if (!binary_path) {
        assert_("Unable to get path");
    }

    FILE *file = fopen(binary_path, "r+");
    if (!file) {
        bool is_root = geteuid() == 0;
        if (uses_application) {
            assert_("Unable to open application (%s)'s executable%s", name, (is_root) ? "" : ". Trying running rmaslr as root");
        }

        assert_("Unable to open file at path %s%s", name, (is_root) ? "" : ". Trying running rmaslr as root");
    }

    struct stat sbuf;
    if (stat(binary_path, &sbuf) != 0) {
        if (uses_application) {
            assert_("Unable to get information on application (%s)'s executable", name);
        }

        assert_("Unable to get information on file at path (%s)", name);
    }

    if (sbuf.st_size < sizeof(struct mach_header_64)) {
        if (uses_application) {
            assert_("Application (%s)'s executable is not a valid mach-o", name);
        }

        assert_("File (%s) is not a valid mach-o", name);
    }

    uint32_t magic;

    fseek(file, 0x0, SEEK_SET);
    fread(&magic, sizeof(uint32_t), 1, file);

    bool is_fat = false;

    switch (magic) {
        case MH_MAGIC:
        case MH_CIGAM:
        case MH_MAGIC_64:
        case MH_CIGAM_64:
            break;
        case FAT_MAGIC:
        case FAT_CIGAM:
        case FAT_MAGIC_64:
        case FAT_CIGAM_64:
            is_fat = true;
            break;
        default: {
            if (uses_application) {
                assert_("Application (%s)'s executable is not a valid mach-o", name);
            }

            assert_("File (%s) is not a valid mach-o", name);
        }
    }

    auto has_aslr = [](struct mach_header header, uint32_t *flags = nullptr) {
        uint32_t flags_ = swap(header.magic, header.flags);
        if (flags) {
            *flags = flags_;
        }

        return (flags_ & MH_PIE) > 0;
    };

    auto remove_aslr = [&file, &name, &has_aslr, &uses_application](long offset, struct mach_header header, const NXArchInfo *archInfo = nullptr) {
        uint32_t flags;
        bool aslr = has_aslr(header, &flags);

        if (!aslr) {
            if (archInfo) {
                fprintf(stdout, "Architecture (%s) does not contain ASLR\n", archInfo->name);
            } else {
                if (uses_application) {
                    error("Application (%s) does not contain ASLR", name);
                } else {
                    error("File (%s) does not contain ASLR", name);
                }
            }

            return false;
        }

        //ask user if should remove ASLR for arm64
        int32_t cputype = swap(header.magic, header.cputype);
        if (cputype == CPU_TYPE_ARM64) {
            bool can_continue = false;
            bool is_valid = false;

            //use std::string and std::cin for safety instead of a risky char* and scanf()
            std::string result;

            while (!is_valid) {
                if (uses_application) {
                    fprintf(stdout, "Removing ASLR on a 64-bit arm application (%s) can result in it crashing. Are you sure you want to continue (y/n): ", name);
                } else {
                    fprintf(stdout, "Removing ASLR on a 64-bit arm file (%s) can result in it crashing. Are you sure you want to continue (y/n): ", name);
                }

                std::cin >> result;

                can_continue = result == "y" || result == "Y";
                is_valid = can_continue || (result == "n" || result == "N");
            }

            if (!can_continue) {
                return false;
            }
        }

        flags &= ~MH_PIE;
        header.flags = swap(header.magic, flags);

        fseek(file, offset, SEEK_SET);
        if (fwrite(&header, sizeof(struct mach_header), 1, file) != 1) {
            error("There was an error writing to file, at-offset %.8lX, errno=%d (%s)", offset, errno, strerror(errno));
        }

        if (archInfo) {
            fprintf(stdout, "Removed ASLR for architecture \"%s\"\n", archInfo->name);
        } else {
            fprintf(stdout, "Successfully Removed ASLR!\n");
        }

        return true;
    };

    auto architectures = std::vector<const NXArchInfo *>();
    auto headers = std::map<long, struct mach_header>();

    if (is_fat) {
        struct fat_header fat;

        fseek(file, 0x0, SEEK_SET);
        fread(&fat, sizeof(struct fat_header), 1, file);

        uint32_t architectures_count = swap(magic, fat.nfat_arch);
        if (!architectures_count) {
            if (uses_application) {
                assert_("Application (%s)'s executable cannot have 0 architectures", name);
            }

            assert_("File (%s) cannot have 0 architectures", name);
        }

        if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
            if (architectures_count * sizeof(struct fat_arch_64) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", name, architectures_count);
                }

                assert_("File (%s) is too small to contain %d architectures", name, architectures_count);
            }

            long current_offset = ftell(file);
            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch_64 arch;
                fread(&arch, sizeof(struct fat_arch_64), 1, file);

                current_offset += sizeof(struct fat_arch_64);
                long header_offset = static_cast<long>(swap(magic, arch.offset));

                const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(swap(magic, arch.cputype), swap(magic, arch.cpusubtype));
                if (!archInfo) {
                    assert_("Architecture at offset %.16lX is not valid", current_offset);
                }

                if (display_archs_only) {
                    architectures.push_back(archInfo);
                    continue;
                }

                auto it = default_architectures.end();
                if (default_architectures.size()) {
                    for (it = default_architectures.begin(); it != default_architectures.end(); it++) {
                        if (*it != archInfo) {
                            continue;
                        }

                        break;
                    }

                    //make sure that the architecture was actually found
                    if (it == default_architectures.end()) {
                        continue;
                    }
                } else if (default_architectures_original_size != 0) {
                    //make sure not to remove aslr when default_architectures has a size less than architecture count in file,
                    //as count would reach 0, and might remove aslr from a non-confirmed architecture
                    continue;
                }

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", name, i + 1);
                    }

                    assert_("File (%s) architecture #%d is placed before its declaration", name, i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", name, i + 1);
                    }

                    assert_("File (%s) architecture #%d is placed past end of file", name, i + 1);
                }

                struct mach_header header;

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                headers.emplace(header_offset, header);
                fseek(file, current_offset, SEEK_SET);
            }
        } else {
            if (architectures_count * sizeof(struct fat_arch) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", name, architectures_count);
                }

                assert_("File (%s) is too small to contain %d architectures", name, architectures_count);
            }

            long current_offset = ftell(file);
            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch arch;
                fread(&arch, sizeof(struct fat_arch), 1, file);

                current_offset += sizeof(struct fat_arch);
                long header_offset = static_cast<long>(swap(magic, arch.offset));

                const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(swap(magic, arch.cputype), swap(magic, arch.cpusubtype));
                if (!archInfo) {
                    assert_("Architecture at offset %.8lX is not valid", current_offset);
                }

                if (display_archs_only) {
                    architectures.push_back(archInfo);
                    continue;
                }

                auto it = default_architectures.end();
                if (default_architectures.size()) {
                    for (it = default_architectures.begin(); it != default_architectures.end(); it++) {
                        if (*it != archInfo) {
                            continue;
                        }

                        break;
                    }

                    //make sure that the architecture was actually found
                    if (it == default_architectures.end()) {
                        continue;
                    }
                } else if (default_architectures_original_size != 0) {
                    //make sure not to remove aslr when default_architectures has a size less than architecture count in file,
                    //as count would reach 0, and might remove aslr from a non-confirmed architecture
                    continue;
                }

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", name, i + 1);
                    }

                    assert_("File (%s) architecture #%d is placed before its declaration", name, i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", name, i + 1);
                    }

                    assert_("File (%s) architecture #%d is placed past end of file", name, i + 1);
                }

                struct mach_header header;

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                headers.emplace(header_offset, header);
                fseek(file, current_offset, SEEK_SET);
            }
        }
    } else {
        struct mach_header header;

        fseek(file, 0x0, SEEK_SET);
        fread(&header, sizeof(struct mach_header), 1, file);

        auto it = default_architectures.end();
        if (default_architectures.size()) {
            const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(swap(header.magic, header.cputype), swap(header.magic, header.cpusubtype));

            for (it = default_architectures.begin(); it != default_architectures.end(); it++) {
                if (*it != archInfo) {
                    continue;
                }

                if (uses_application) {
                    notice("Application (%s) does not contain multiple architectures, but the executable is of type \"%s\", so ASLR removal will commence", name, archInfo->name);
                } else {
                    notice("File (%s) does not contain multiple architectures, but the executable is of type \"%s\", so ASLR removal will commence", name, archInfo->name);
                }

                break;
            }

            if (it == default_architectures.end()) {
                if (uses_application) {
                    notice("Application (%s) does not contain multiple architectures, but you can still specify architecture \"%s\" to remove ASLR from the executable", name, archInfo->name);
                } else {
                    notice("File (%s) does not contain multiple architectures, but you can still specify architecture \"%s\" to remove ASLR from it", name, archInfo->name);
                }

                return -1;
            }
        }

        headers.emplace(0x0, header);
    }

    if (display_archs_only) {
        auto size = architectures.size();
        if (size) {
            if (uses_application) {
                fprintf(stdout, "Application (%s) contains %ld architectures:\n", name, size);
            } else {
                fprintf(stdout, "File (%s) contains %ld architectures:\n", name, size);
            }

            for (const NXArchInfo *archInfo : architectures) {
                fprintf(stdout, "%s\n", archInfo->name);
            }
        } else {
            if (uses_application) {
                assert_("Application (%s) is not fat", name);
            } else {
                assert_("File (%s) is not fat", name);
            }
        }

        return 0;
    }

    if (check_aslr_only) {
        for (const auto& item : headers) {
            struct mach_header header = item.second;

            uint32_t cputype = swap(header.magic, header.cputype);
            uint32_t cpusubtype = swap(header.magic, header.cpusubtype);

            const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(cputype, cpusubtype);
            bool aslr = has_aslr(header);

            fprintf(stdout, "Architecture (%s) %s ASLR", archInfo->name, (aslr ? "contains" : "does not contain"));
            if (aslr && cputype == CPU_TYPE_ARM64) {
                fprintf(stdout, " (Removing ASLR can cause crashes)");
            }

            fprintf(stdout, "\n");
        }

        return 0;
    }

    bool removed_aslr = false;

    auto size = headers.size();
    for (const auto& item : headers) {
        long offset = item.first;
        struct mach_header header = item.second;

        uint32_t cputype = swap(header.magic, header.cputype);
        uint32_t cpusubtype = swap(header.magic, header.cpusubtype);

        bool is_thin = size < 2;
        const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(cputype, cpusubtype);

        if (is_thin && !default_architectures.size()) { //also displays fat files with less than 2 archs as non-fat
            archInfo = nullptr;
        }

        bool removed_aslr_ = remove_aslr(offset, header, archInfo);
        if (!removed_aslr && removed_aslr_) {
            removed_aslr = removed_aslr_;
        }

        auto iter = std::find(default_architectures.begin(), default_architectures.end(), archInfo);
        if (iter != default_architectures.end()) {
            default_architectures.erase(iter);
        }
    }

    if (removed_aslr) {
        if (uses_application) {
            notice("Application (%s) may not run til you have signed its executable (at path %s) (preferably with ldid)", name, binary_path);
        } else {
            notice("File may not run til you have signed it (preferably with ldid)");
        }
    }

    if (default_architectures.size()) {
        const NXArchInfo * archInfo = default_architectures.front();
        char const * architectures = archInfo->name;

        if (default_architectures.size() > 1) {
            //only create std::string on demand to allow easy appending of string

            //show the first element, and then add comma
            //then add the rest via for loop, this makes sure
            //there isn't an extra comma
            std::string architectures_ = architectures;
            default_architectures.erase(default_architectures.begin());

            for (const auto& archInfo : default_architectures) {
                architectures_.append(", ");
                architectures_.append(archInfo->name);
            }

            architectures = strdup(architectures_.c_str());
        }

        assert_("Unable to find & remove ASLR from architecture(s) \"%s\"", architectures);
    }

    fclose(file);
}
