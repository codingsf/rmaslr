#include <CoreFoundation/CoreFoundation.h>

#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>

#include <sys/stat.h>

#include <iostream>
#include <map>

#include <string>
#include <vector>

#include <unistd.h>

#define assert_(str, ...) fprintf(stderr, "\x1B[31mError:\x1B[0m " str "\n", ##__VA_ARGS__); return -1

#define notice(str, ...) fprintf(stdout, "\x1B[33mNotice:\x1B[0m " str "\n", ##__VA_ARGS__);
#define error(str, ...) fprintf(stderr, "\x1B[31mError:\x1B[0m " str "\n", ##__VA_ARGS__); exit(0)

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


namespace std {
    bool is_in_map(const std::vector<std::map<const char *, std::string>>& vector, const std::string& value) noexcept;
    bool case_compare(const std::string& first, const std::string& second) noexcept;

    std::string find_last_component(const std::string& string) noexcept;
}

namespace rmaslr {
    struct options {
    public:
        inline static bool application() {
            return application_;
        }

        inline static bool application(bool new_value) {
            return application_ = new_value;
        }

        inline static bool display_archs() {
            return display_archs_;
        }

        inline static bool display_archs(bool new_value) {
            return display_archs_ = new_value;
        }

        inline static bool check_aslr() {
            return check_aslr_;
        }

        inline static bool check_aslr(bool new_value) {
            return check_aslr_ = new_value;
        }
    private:
        static bool application_;
        static bool display_archs_;
        static bool check_aslr_;
    };

    class platform {
    public:
        static inline bool iphoneos() noexcept {
            return get_platform() == "iPhone OS";
        }

        static inline bool macosx() noexcept {
            return get_platform() == "Mac OS X";
        }
    private:
        static std::string get_platform() noexcept;
        static std::string load_from_filesystem() noexcept;
    };

    inline bool is_root() noexcept {
        return geteuid() == 0;
    }

    template <typename T>
    T request_input(std::string question, std::vector<T> values = std::vector<T>()) {
        T input;

        auto is_valid = [&values](T input) {
            if (!values.size()) {
                return true;
            }

            for (const auto& value : values) {
                if (input != value) {
                    continue;
                }

                return true;
            }

            return false;
        };

        do {
            std::cout << question;
            std::cin >> input;
        } while (!is_valid(input));

        return input;
    }

    template <typename T>
    T request_input_ranged(std::string question, std::pair<T, T> range) {
        T input;

        do {
            std::cout << question;
            std::cin >> input;
        } while (input < range.first || input > range.second);

        return input;
    }

    size_t get_size(size_t size) noexcept;

    uint32_t swap(uint32_t magic, uint32_t value) noexcept;
    uint64_t swap(uint32_t magic, uint64_t value) noexcept;

    __attribute__((unused))
    static inline int32_t swap(uint32_t magic, int32_t value) noexcept {
        return static_cast<int32_t>(swap(magic, static_cast<uint32_t>(value)));
    }

    __attribute__((unused))
    static inline int64_t swap(uint32_t magic, int64_t value) noexcept {
        return static_cast<int64_t>(swap(magic, static_cast<uint64_t>(value)));
    }

    __printflike(1, 2)
    std::string formatted_string(const char *string, ...) noexcept;
    std::map<const char *, std::string> parse_application_container(const std::string& path) noexcept;

    class file {
    public:
        file(const char *path, const char *mode = "r+");
        file(FILE *file);

        inline ~file() noexcept {
            fclose(file_);
        }

        inline long position() const noexcept {
            return position_;
        }

        enum class seek_type {
            origin,
            current,
            end
        };

        void seek(long position, seek_type seek) noexcept;

        template<typename T>
        T read() noexcept {
            T *buffer = static_cast<T *>(malloc(sizeof(T)));
            if (!buffer) {
                error("file::read(); Unable to allocate buffer needed to read data of size %ld, errno=%d(%s)", sizeof(T), errno, strerror(errno));
            }

            if (fread(buffer, sizeof(T), 1, file_) != 1) {
                error("file::read(); Unable to read from file at offset %.16lX, errno=%d(%s)", position_, errno, strerror(errno));
            }

            position_ += sizeof(T);
            return *buffer;
        }

        template <typename T>
        inline void write(T buff) {
            write_to_file<T>::write(*this, buff);
        }

        inline FILE *get_file() const noexcept {
            return file_;
        }

        inline struct stat stat() const noexcept {
            return sbuf_;
        }
    private:
        FILE *file_ = nullptr;
        long position_ = 0x0;

        struct stat sbuf_ = {};

        template<typename T>
        struct write_to_file {
            friend class file;
            static void write(const file& file, T buff) {
                if (fwrite(static_cast<void *>(&buff), sizeof(T), 1, file.file_) != 1) {
                    error("file::write(); Unable to write to file at position (%ld), errno=%d(%s)", file.position_, errno, strerror(errno));
                }
            }
        };

        template<typename T>
        struct write_to_file<T*> {
            friend class file;
            static void write(const file& file, T buff) {
                if (fwrite(static_cast<void *>(buff), sizeof(T), 1, file.file_) != 1) {
                    error("file::write(); Unable to write to file at position (%ld), errno=%d(%s)", file.position_, errno, strerror(errno));
                }
            }
        };
    };
}
