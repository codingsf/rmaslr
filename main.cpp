//
//  main.cpp
//  rmaslr
//
//  Created by iNoahDev on 5/10/16.
//  Copyright © 2016 iNoahDev. All rights reserved.
//

#include <cstddef>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>

#include <sys/stat.h>
#include <unistd.h>

extern "C" CFArrayRef SBSCopyApplicationDisplayIdentifiers(bool onlyActive, bool debugging);
extern "C" CFStringRef SBSCopyLocalizedApplicationNameForDisplayIdentifier(CFStringRef bundle_id);
extern "C" CFStringRef SBSCopyExecutablePathForDisplayIdentifier(CFStringRef bundle_id);

#define assert_(str, ...) fprintf(stderr, "\x1B[31mError:\x1B[0m " str "\n", ##__VA_ARGS__); return -1
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

static inline uint32_t swap(uint32_t magic, uint32_t value) {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = ((value >> 8) & 0x00ff00ff) | ((value << 8) & 0xff00ff00);
        value = ((value >> 16) & 0x0000ffff) | ((value << 16) & 0xffff0000);
    }

    return value;
}

static inline uint64_t swap(uint32_t magic, uint64_t value) {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM || magic == FAT_CIGAM_64) {
        value = (value & 0x00000000ffffffff) << 32 | (value & 0xffffffff00000000) >> 32;
        value = (value & 0x0000ffff0000ffff) << 16 | (value & 0xffff0000ffff0000) >> 16;
        value = (value & 0x00ff00ff00ff0ff) << 8  | (value & 0xff00ff00ff00ff00) >> 8;
    }

    return value;
}

void print_usage() {
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

//this can't be a c++ lambda?
CFComparisonResult in_case_sensitive_compare(const CFStringRef string1, const CFStringRef string2, __attribute__((unused)) void *context) {
    return CFStringCompare(string1, string2, kCFCompareCaseInsensitive);
}

int main(int argc, const char * argv[], const char * envp[]) {
    if (argc < 2) {
        print_usage();
    }

    //for fancy debug messages
    bool uses_application = false;

    bool display_archs_only = false;
    bool check_aslr_only = false;

    const char *name = nullptr;
    const char *binary_path = nullptr;

    std::vector<const NXArchInfo *> default_architectures;
    auto default_architectures_original_size = 0;

    const char *argument = argv[1];
    if (argument[0] != '-') {
        assert_("%s is not an option", argument);
    }

    const char *option = &argument[1];
    if (option[0] == '-') {
        option++;
    }

    if (strcmp(option, "?") == 0 || strcmp(option, "h") == 0) {
        if (argc > 2) {
            assert_("Please run %s seperately", argument);
        }

        print_usage();
    } else if (strcmp(option, "apps") == 0 || strcmp(option, "-applications") == 0) {
        if (argc > 2) {
            assert_("Please run %s seperately", argument);
        }

        CFArrayRef applications = SBSCopyApplicationDisplayIdentifiers(false, false);
        if (!applications) {
            assert_("Unable to retrieve application-list");
        }

        CFIndex applications_count = CFArrayGetCount(applications);
        CFMutableArrayRef sorted_applications = CFArrayCreateMutable(CFAllocatorGetDefault(), applications_count, nullptr);

        for (CFIndex i = 0; i < applications_count; i++) {
            CFStringRef bundle_id = (CFStringRef)CFArrayGetValueAtIndex(applications, i);
            CFArrayAppendValue(sorted_applications, SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundle_id));
        }

        CFArraySortValues(sorted_applications, CFRangeMake(0, applications_count), (CFComparatorFunction)in_case_sensitive_compare, nullptr);
        fprintf(stdout, "%s", CFStringGetCStringPtr((CFStringRef)CFArrayGetValueAtIndex(sorted_applications, 0), kCFStringEncodingUTF8));

        for (CFIndex i = 1; i < applications_count; i++) {
            const char *display_name = CFStringGetCStringPtr((CFStringRef)CFArrayGetValueAtIndex(sorted_applications, i), kCFStringEncodingUTF8);
            if (!display_name) { //why does this happen?
                continue;
            }

            fprintf(stdout, ", %s", display_name);
        }

        fprintf(stdout, "\n");
        return 0;
    } else if (strcmp(option, "archs") == 0 || strcmp(option, "archs") == 0) {
        if (argc > 2) {
            assert_("Please run %s seperately", argument);
        }

        //to sort alphabetically
        std::vector<const char *> arch_names;

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
            name = argv[i];

            CFStringRef application = CFStringCreateWithCString(CFAllocatorGetDefault(), name, kCFStringEncodingUTF8);
            CFArrayRef apps = SBSCopyApplicationDisplayIdentifiers(false, false);

            if (!apps) {
                assert_("Unable to retrieve application-list");
            }

            CFStringRef executable_path = nullptr;
            CFIndex count = CFArrayGetCount(apps);

            CFStringRef slash = CFStringCreateWithCString(CFAllocatorGetDefault(), "/", kCFStringEncodingUTF8);

            for (CFIndex i = 0; i < count; i++) {
                CFStringRef bundle_id = (CFStringRef)CFArrayGetValueAtIndex(apps, i);
                CFStringRef display_name = SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundle_id);
                CFStringRef executable_path_ = SBSCopyExecutablePathForDisplayIdentifier(bundle_id);

                executable_path = executable_path_;

                if (CFStringCompare(application, bundle_id, 0) == kCFCompareEqualTo) {
                    break;
                } else if (CFStringCompare(application, display_name, 0) == kCFCompareEqualTo) {
                    break;
                }

                //compare backwards to find last '/'
                CFIndex executable_path_length = CFStringGetLength(executable_path_);

                CFRange range = CFRangeMake(0, executable_path_length);
                if (!CFStringFindWithOptions(executable_path_, slash, CFRangeMake(0, executable_path_length), kCFCompareBackwards, &range)) {
                    continue;
                }

                CFIndex location = executable_path_length - range.location;
                CFStringRef executable_name = CFStringCreateWithSubstring(CFAllocatorGetDefault(), executable_path_, CFRangeMake(location, executable_path_length - location));

                if (CFStringCompare(application, executable_name, 0) == kCFCompareEqualTo) {
                    break;
                }

                executable_path = nullptr;
            }

            if (!executable_path) {
                assert_("Unable to find application %s", name);
            }

            uses_application = true;
            binary_path = CFStringGetCStringPtr(executable_path, kCFStringEncodingUTF8);

            if (access(binary_path, R_OK) != 0) {
                const char *application_string = CFStringGetCStringPtr(application, kCFStringEncodingUTF8);
                if (access(binary_path, F_OK) != 0) {
                    assert_("Application (%s)'s executable does not exist", application_string);
                }

                assert_("Unable to read Application (%s)'s executable", application_string);
            }
        } else if (strcmp(option, "b") == 0 || strcmp(option, "binary") == 0) {
            if (last_argument) {
                assert_("Please provide a path to a mach-o binary");
            }

            i++;
            std::string path = argv[i];

            if (path[0] != '/') {
                size_t size = 4096;
                char *buf = new char[size];

                char *result = getcwd(buf, size);
                if (!result) {
                    if (errno == ERANGE) {
                        do {
                            size += 8;

                            buf = new char[size];
                            result = getcwd(buf, size);
                        } while (!result && errno == ERANGE);
                    } else {
                        assert_("Unable to get the current working directory, errno=%d (%s)", errno, strerror(errno));
                    }
                }

                //use std::string for safety (so we don't end up writing outside buf, while trying to append a '/')
                std::string buffer = buf;

                //unable to use back() even though we're on c++14
                if (*(buffer.end() - 1) != '/') {
                    buffer.append(1, '/');
                }

                path.insert(0, buffer);
            }

            binary_path = path.c_str();
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
                assert_("Cannot both display and select an arch to remove ASLR from");
            }

            if (display_archs_only) {
                assert_("rmaslr is already configured to print architectures");
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

        assert_("Unable to open file at path %s%s", name,(is_root) ? "" : ". Trying running rmaslr as root");
    }

    struct stat sbuf;
    if (stat(binary_path, &sbuf) != 0) {
        if (uses_application) {
            assert_("Unable to get information on application (%s)'s executable", name);
        }

        assert_("Unable to get information on file at path %s", name);
    }

    if (sbuf.st_size < sizeof(struct mach_header_64)) {
        if (uses_application) {
            assert_("Application (%s)'s executable is not a valid mach-o", name);
        }

        assert_("File at path (%s) is not a valid mach-o", name);
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

            assert_("File at path (%s) is not a valid mach-o", name);
        }
    }

    struct mach_header header;
    long header_offset = 0x0;

    auto has_aslr = [&header](uint32_t *flags = nullptr) {
        uint32_t flags_ = swap(header.magic, header.flags);
        if (flags) {
            *flags = flags_;
        }

        return (flags_ & MH_PIE) > 0;
    };

    auto remove_aslr = [&file, &name, &header, &header_offset, &has_aslr, &uses_application](const NXArchInfo *archInfo = nullptr) {
        uint32_t flags;
        bool aslr = has_aslr(&flags);

        if (!aslr) {
            if (archInfo) {
                fprintf(stdout, "Architecture (%s) does not contain ASLR\n", archInfo->name);
            } else {
                if (uses_application) {
                    fprintf(stdout, "Application (%s) does not contain ASLR\n", name);
                } else {
                    fprintf(stdout, "File does not contain ASLR\n");
                }
            }

            return false;
        }

        //ask user if should remove ASLR for arm64
        uint32_t cputype = swap(header.magic, static_cast<uint32_t>(header.cputype));
        if (cputype == CPU_TYPE_ARM64) {
            bool can_continue = false;
            bool is_valid = false;

            char *result = new char[2];
            memset(result, '\0', 2); //zero out the string for safety purposes

            while (!is_valid) {
                fprintf(stdout, "Removing ASLR on a 64-bit arm %s can result in it crashing. Are you sure you want to continue (y/n): ", (archInfo) ? "file" : "application");
                scanf("%1s", result);

                can_continue = strcasecmp(result, "y") == 0;
                is_valid = can_continue || strcasecmp(result, "n") == 0;
            }

            if (!can_continue) {
                return false;
            }
        }

        flags &= ~MH_PIE;
        header.flags = swap(header.magic, flags);

        fseek(file, header_offset, SEEK_SET);
        if (fwrite(&header, sizeof(struct mach_header), 1, file) != 1) {
            error("There was an error writing to file, at-offset %.8lX, errno=%d", header_offset, errno);
        }

        if (archInfo) {
            fprintf(stdout, "Removed ASLR for architecture \"%s\"\n", archInfo->name);
        } else {
            fprintf(stdout, "Successfully Removed ASLR!\n");
        }

        return true;
    };

    bool had_aslr = false;

    if (is_fat) {
        struct fat_header fat;

        fseek(file, 0x0, SEEK_SET);
        fread(&fat, sizeof(struct fat_header), 1, file);

        uint32_t architectures_count = swap(magic, fat.nfat_arch);
        if (!architectures_count) {
            if (uses_application) {
                assert_("Application (%s)'s executable cannot have 0 architectures", name);
            }

            assert_("File at path (%s) cannot have 0 architectures", name);
        }

        if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
            if (architectures_count * sizeof(struct fat_arch_64) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", name, architectures_count);
                }

                assert_("File at path (%s) is too small to contain %d architectures", name, architectures_count);
            }

            if (display_archs_only) {
                if (uses_application) {
                    fprintf(stdout, "Application (%s) contains %d architectures:\n", name, architectures_count);
                } else {
                    fprintf(stdout, "File contains %d architectures:\n", architectures_count);
                }
            }

            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch_64 arch;
                fread(&arch, sizeof(struct fat_arch_64), 1, file);

                long current_offset = ftell(file);
                header_offset = static_cast<long>(swap(magic, arch.offset));

                const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(swap(magic, static_cast<uint32_t>(arch.cputype)), swap(magic, static_cast<uint32_t>(arch.cpusubtype)));
                if (!archInfo) {
                    assert_("Architecture at offset %.16lX is not valid", current_offset);
                }

                if (display_archs_only) {
                    fprintf(stdout, "%s\n", archInfo->name);
                    continue;
                }

                //can't use auto here :/
                auto it = default_architectures.end();
                if (default_architectures.size()) {
                    for (it = default_architectures.begin(); it != default_architectures.end(); it++) {
                        if (*it != archInfo) {
                            continue;
                        }

                        break;
                    }

                    if (it == default_architectures.end()) {
                        continue;
                    }
                } else if (default_architectures_original_size != 0) {
                    //make sure not remove aslr when default_architectures has a size less than architecture count in file,
                    //as count would reach 0, and might remove aslr from a non-confirmed architecture
                    continue;
                }

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", name, i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed before its declaration", name, i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", name, i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed past end of file", name, i + 1);
                }

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                if (check_aslr_only) {
                    bool aslr = has_aslr();

                    fprintf(stdout, "Architecture (%s) does%s contain ASLR", archInfo->name, (aslr ? "" : " not"));
                    if (aslr && header.cputype == CPU_TYPE_ARM64) {
                        fprintf(stdout, " (Removing ASLR can cause crashes)");
                    }

                    fprintf(stdout, "\n");
                } else {
                    if (architectures_count < 2 && !default_architectures.size()) { //display fat files with less than 2 archs as non-fat
                        archInfo = nullptr;
                    }

                    bool had_aslr_ = remove_aslr(archInfo);
                    if (!had_aslr) {
                        had_aslr = had_aslr_;
                    }

                    if (it != default_architectures.end()) {
                        default_architectures.erase(it);
                    }
                }

                fseek(file, current_offset, SEEK_SET);
            }

            if (display_archs_only) {
                return 0;
            }
        } else {
            if (architectures_count * sizeof(struct fat_arch) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", name, architectures_count);
                }

                assert_("File at path (%s) is too small to contain %d architectures", name, architectures_count);
            }

            if (display_archs_only) {
                if (uses_application) {
                    fprintf(stdout, "Application (%s) contains %d architectures:\n", name, architectures_count);
                } else {
                    fprintf(stdout, "File contains %d architectures:\n", architectures_count);
                }
            }

            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch arch;
                fread(&arch, sizeof(struct fat_arch), 1, file);

                long current_offset = ftell(file);
                header_offset = static_cast<long>(swap(magic, arch.offset));

                const NXArchInfo *archInfo = NXGetArchInfoFromCpuType(swap(magic, static_cast<uint32_t>(arch.cputype)), swap(magic, static_cast<uint32_t>(arch.cpusubtype)));
                if (!archInfo) {
                    assert_("Architecture at offset %.8lX is not valid", current_offset);
                }

                if (display_archs_only) {
                    fprintf(stdout, "%s\n", archInfo->name);
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

                    if (it == default_architectures.end()) {
                        continue;
                    }
                } else if (default_architectures_original_size != 0) {
                    //make sure not remove aslr when default_architectures has a size less than architecture count in file,
                    //as count would reach 0, and might remove aslr from a non-confirmed architecture
                    continue;
                }

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", name, i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed before its declaration", name, i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", name, i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed past end of file", name, i + 1);
                }

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                if (check_aslr_only) {
                    bool aslr = has_aslr();

                    fprintf(stdout, "Architecture (%s) does%s contain ASLR", archInfo->name, (aslr ? "" : " not"));
                    if (aslr && header.cputype == CPU_TYPE_ARM64) {
                        fprintf(stdout, " (Removing ASLR can cause crashes)");
                    }

                    fprintf(stdout, "\n");
                } else {
                    if (architectures_count < 2 && !default_architectures.size()) { //display fat files with less than 2 archs as non-fat
                        archInfo = nullptr;
                    }

                    bool had_aslr_ = remove_aslr(archInfo);
                    if (!had_aslr) {
                        had_aslr = had_aslr_;
                    }

                    if (it != default_architectures.end()) {
                        default_architectures.erase(it);
                    }
                }

                fseek(file, current_offset, SEEK_SET);
            }

            if (display_archs_only) {
                return 0;
            }
        }
    } else {
        if (display_archs_only || default_architectures.size()) {
            if (uses_application) {
                fprintf(stdout, "Application (%s) is not fat\n", name);
            } else {
                fprintf(stdout, "File is not fat\n");
            }

            return 0;
        }

        fseek(file, 0x0, SEEK_SET);
        fread(&header, sizeof(struct mach_header), 1, file);

        if (check_aslr_only) {
            if (uses_application) {
                fprintf(stdout, "Application (%s) does%s contain ASLR\n", name, (has_aslr() ? "" : " not"));
            } else {
                fprintf(stdout, "File does%s contain ASLR\n", (has_aslr() ? "" : " not"));
            }

            return 0;
        }

        //don't set header_offset since it's already 0x0
        remove_aslr();
    }

    if (had_aslr) {
        if (uses_application) {
            fprintf(stdout, "\x1B[33mNote:\x1B[0m Application (%s) may not run til you have signed its executable (at path %s) (preferably with ldid)\n", name, binary_path);
        } else {
            fprintf(stdout, "\x1B[33mNote:\x1B[0m File may not run til you have signed it (preferably with ldid)\n");
        }
    }

    if (default_architectures.size()) {
        std::string architectures = default_architectures.front()->name;

        if (default_architectures.size() > 1) {
            architectures.append(", ");
            default_architectures.erase(default_architectures.begin());

            for (const auto& archInfo : default_architectures) {
                architectures.append(", ");
                architectures.append(archInfo->name);
            }
        }

        assert_("Unable to find & remove ASLR from architecture(s) \"%s\"", architectures.c_str());
    }

    fclose(file);
}
