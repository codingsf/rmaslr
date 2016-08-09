//
//  main.cpp
//  rmaslr
//
//  Created by Anonymous on 5/10/16.
//  Copyright © 2016 iNoahDev. All rights reserved.
//

#include <cstddef>
#include <string>

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
        value = (value & 0x00ff00ff00ff00ff) << 8  | (value & 0xff00ff00ff00ff00) >> 8;
    }

    return value;
}

void print_usage() {
    fprintf(stdout, "Usage: rmaslr -a application\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "    -a,     --app/--application,   Remove ASLR for an application\n");
    fprintf(stdout, "    -apps,  --applications,        Print a list of Applications\n");
    fprintf(stdout, "    -b,     --binary,              Remove ASLR for a Mach-O Executable\n");
    fprintf(stdout, "    -?/-h,  --help,                Print this message\n");

    exit(0);
}

//this can't be a c++ lambda?
CFComparisonResult in_case_sensitive_compare(const CFStringRef string1, const CFStringRef string2, __attribute__((unused)) void *context) {
    return CFStringCompare(string1, string2, kCFCompareCaseInsensitive);
}

int main(int argc, const char * argv[], const char * envp[]) {
    if (argc < 2) {
        print_usage();
    } else if (argc > 3) {
        assert_("Too many arguments provided");
    }

    const char *argument = argv[1];
    if (argument[0] != '-') {
        assert_("%s is not an option", argument);
    }

    const char *option = &argument[1];
    if (option[0] == '-') {
        option++;
    }

    if (strcmp(option, "?") == 0 || strcmp(option, "h") == 0) {
        print_usage();
    } else if (strcmp(option, "apps") == 0 || strcmp(option, "-applications") == 0) {
        CFArrayRef applications = SBSCopyApplicationDisplayIdentifiers(false, false);
        CFIndex applications_count = CFArrayGetCount(applications);

        CFMutableArrayRef sorted_applications = CFArrayCreateMutable(CFAllocatorGetDefault(), applications_count, nullptr);

        for (CFIndex i = 0; i < CFArrayGetCount(applications); i++) {
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
    }

    //for fancy debug messages
    bool uses_application = false;
    const char *install_path = nullptr;

    if (strcmp(option, "a") == 0 || strcmp(option, "app") == 0 || strcmp(option, "application") == 0) {
        if (argc < 3) {
            assert_("Please provide an application display-name/identifier/executable-name");
        }

        CFStringRef application = CFStringCreateWithCString(CFAllocatorGetDefault(), argv[2], kCFStringEncodingUTF8);
        CFArrayRef apps = SBSCopyApplicationDisplayIdentifiers(false, false);

        CFStringRef executable_path = nullptr;
        CFIndex count = CFArrayGetCount(apps);

        CFStringRef slash = CFStringCreateWithCString(CFAllocatorGetDefault(), "/", kCFStringEncodingUTF8);

        for (CFIndex i = 0; i < count; i++) {
            CFStringRef bundle_id = (CFStringRef)CFArrayGetValueAtIndex(apps, i);
            CFStringRef display_name = SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundle_id);
            CFStringRef executable_path_ = SBSCopyExecutablePathForDisplayIdentifier(bundle_id);

            //compare backwards to find last '/'
            CFIndex executable_path_length = CFStringGetLength(executable_path_);

            CFRange range = CFRangeMake(0, executable_path_length);
            if (!CFStringFindWithOptions(executable_path_, slash, CFRangeMake(0, executable_path_length), kCFCompareBackwards, &range)) {
                continue;
            }

            CFIndex location = executable_path_length - range.location;

            CFStringRef executable_name = CFStringCreateWithSubstring(CFAllocatorGetDefault(), executable_path_, CFRangeMake(location, executable_path_length - location));
            executable_path = executable_path_;

            if (CFStringCompare(application, bundle_id, 0) == kCFCompareEqualTo) {
                break;
            } else if (CFStringCompare(application, display_name, 0) == kCFCompareEqualTo) {
                break;
            } else if (CFStringCompare(application, executable_name, 0) == kCFCompareEqualTo) {
                break;
            }

            executable_path = nullptr;
        }

        if (!executable_path) {
            assert_("Unable to find application %s", argv[2]);
        }

        uses_application = true;
        install_path = CFStringGetCStringPtr(executable_path, kCFStringEncodingUTF8);

        if (access(install_path, R_OK) != 0) {
            const char *application_string = CFStringGetCStringPtr(application, kCFStringEncodingUTF8);
            if (access(install_path, F_OK) != 0) {
                assert_("Application (%s)'s executable does not exist", application_string);
            }

            assert_("Unable to read Application (%s)'s executable", application_string);
        }

    } else if (strcmp(option, "b") == 0 || strcmp(option, "binary") == 0) {
        if (argc < 3) {
            assert_("Please provide a path to a mach-o binary");
        }

        std::string path = argv[2];
        if (path[0] != '/') {
            char *buf = new char[4096];
            if (!getcwd(buf, 4096)) {
                assert_("Unable to get the current directory, is the current directory deleted?");
            }

            //use std::string for safety (so we don't end up writing outside buf, while trying to append a '/')
            std::string buffer = buf;

            //unable to use back() even though we're on c++14
            if (*(buffer.end() - 1) != '/') {
                buffer.append(1, '/');
            }

            path.insert(0, buffer);
        }

        install_path = path.c_str();
        if (access(install_path, R_OK) != 0) {
            if (access(install_path, F_OK) != 0) {
                assert_("File at path (%s) does not exist", install_path);
            }

            assert_("Unable to read file at path (%s)", install_path);
        }
    } else {
        assert_("Unrecognized option %s", argument);
    }

    if (!install_path) {
        assert_("Unable to get path");
    }

    FILE *file = fopen(install_path, "r+");
    if (!file) {
        if (uses_application) {
            assert_("Unable to open application (%s)'s executable", argv[2]);
        }

        assert_("Unable to open file at path %s", argv[2]);
    }

    struct stat sbuf;
    if (stat(install_path, &sbuf) != 0) {
        if (uses_application) {
            assert_("Unable to get information on application (%s)'s executable", argv[2]);
        }

        assert_("Unable to get information on file at path %s", argv[2]);
    }

    if (sbuf.st_size < sizeof(struct mach_header_64)) {
        if (uses_application) {
            assert_("Application (%s)'s executable is not a valid mach-o", argv[2]);
        }

        assert_("File at path (%s) is not a valid mach-o", argv[2]);
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
                assert_("Application (%s)'s executable is not a valid mach-o", argv[2]);
            }

            assert_("File at path (%s) is not a valid mach-o", argv[2]);
        }
    }

    struct mach_header header;
    long header_offset = 0x0;

    auto remove_aslr = [&file, &argv, &header, &header_offset, &uses_application](const NXArchInfo *archInfo = nullptr) {
        uint32_t flags = swap(header.magic, header.flags);
        if (!(flags & MH_PIE)) {
            if (archInfo) {
                fprintf(stdout, "Architecture %s does not contain aslr\n", archInfo->name);
            } else {
                if (uses_application) {
                    fprintf(stdout, "Application (%s) does not contain aslr\n", argv[2]);
                } else {
                    fprintf(stdout, "File does not contain aslr\n");
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
            memset(static_cast<void *>(result), '\0', 2); //zero out the string for safety purposes

            while (!is_valid) {
                fprintf(stdout, "Removing ASLR on a 64-bit arm %s can result in it crashing. Are you sure you want to continue (y/n): ", (archInfo) ? "file" : "application");
                scanf("%1s", result);

                can_continue = strcasecmp(result, "y") == 0;
                is_valid = can_continue || strcasecmp(result, "n") == 0;
            }

            if (!can_continue) {
                //even though result should be whether or not file had aslr, return false so that the "sign file" message does not show (because nothing happened)
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
                assert_("Application (%s)'s executable cannot have 0 architectures", argv[2]);
            }

            assert_("File at path (%s) cannot have 0 architectures", argv[2]);
        }

        if (magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
            if (architectures_count * sizeof(struct fat_arch_64) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", argv[2], architectures_count);
                }

                assert_("File at path (%s) is too small to contain %d architectures", argv[2], architectures_count);
            }

            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch_64 arch;
                fread(&arch, sizeof(struct fat_arch_64), 1, file);

                long current_offset = ftell(file);
                header_offset = static_cast<long>(swap(magic, arch.offset));

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", argv[2], i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed before its declaration", argv[2], i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", argv[2], i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed past end of file", argv[2], i + 1);
                }

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                const NXArchInfo *archInfo = nullptr;
                if (architectures_count > 1) { //display fat files with only 1 arch as non-fat
                    archInfo = NXGetArchInfoFromCpuType(swap(magic, static_cast<uint32_t>(arch.cputype)), swap(magic, static_cast<uint32_t>(arch.cpusubtype)));
                }

                bool had_aslr_ = remove_aslr(archInfo);
                if (!had_aslr) {
                    had_aslr = had_aslr_;
                }

                fseek(file, current_offset, SEEK_SET);
            }
        } else {
            if (architectures_count * sizeof(struct fat_arch) > sbuf.st_size) {
                if (uses_application) {
                    assert_("Application (%s)'s executable is too small to contain %d architectures", argv[2], architectures_count);
                }

                assert_("File at path (%s) is too small to contain %d architectures", argv[2], architectures_count);
            }

            for (uint32_t i = 0; i < architectures_count; i++) {
                struct fat_arch arch;
                fread(&arch, sizeof(struct fat_arch), 1, file);

                long current_offset = ftell(file);
                header_offset = static_cast<long>(swap(magic, arch.offset));

                //basic validation
                if (header_offset < current_offset) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed before its declaration", argv[2], i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed before its declaration", argv[2], i + 1);
                }

                if (header_offset > sbuf.st_size) {
                    if (uses_application) {
                        assert_("Application (%s) executable's architecture #%d is placed past end of file", argv[2], i + 1);
                    }

                    assert_("File at path (%s) architecture #%d is placed past end of file", argv[2], i + 1);
                }

                fseek(file, header_offset, SEEK_SET);
                fread(&header, sizeof(struct mach_header), 1, file);

                const NXArchInfo *archInfo = nullptr;
                if (architectures_count > 1) { //display fat files with only 1 arch as non-fat
                    archInfo = NXGetArchInfoFromCpuType(swap(magic, static_cast<uint32_t>(arch.cputype)), swap(magic, static_cast<uint32_t>(arch.cpusubtype)));
                }

                bool had_aslr_ = remove_aslr(archInfo);
                if (!had_aslr) {
                    had_aslr = had_aslr_;
                }

                fseek(file, current_offset, SEEK_SET);
            }
        }
    } else {
        fseek(file, 0x0, SEEK_SET);
        fread(&header, sizeof(struct mach_header), 1, file);

        //don't have to set header_offset since it's already 0x0
        had_aslr = remove_aslr();
    }

    if (had_aslr) {
        fprintf(stdout, "Note: %s (%s) may not run til you have signed %s (preferably with ldid)\n", uses_application ? "application" : "file at path", argv[2], uses_application ? "its executable" : "it");
    }

    fclose(file);
}
