//
//  main.cpp
//  rmaslr
//
//  Created by Anonymous on 5/10/16.
//  Copyright © 2016 iNoahDev. All rights reserved.
//

#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <iostream>
#include <map>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

#define assert_(str) std::cout << str << std::endl; return -1
#define error(str) std::cout << "\x1b[31mError:\x1b[0m " << str << std::endl; return -1

#ifdef DEBUG
#define log(str) std::cout << "DEBUG: " << str << std::endl;
#else
#define log(str) 
#endif

#define MH_MAGIC 0xfeedface
#define MH_CIGAM 0xcefaedfe

#define MH_MAGIC_64 0xfeedfacf
#define MH_CIGAM_64 0xcffaedfe

struct fat_header {
    uint32_t magic;
    uint32_t nfat_arch;
};

#define FAT_MAGIC 0xcafebabe
#define FAT_CIGAM 0xbebafeca

struct fat_arch {
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t offset;
    uint32_t size;
    uint32_t align;
};

struct mach_header {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
};

#define MH_PIE 0x200000

bool swap = false;

static uint32_t Swap_(uint32_t value) {
    value = ((value >> 8) & 0x00ff00ff) | ((value << 8) & 0xff00ff00);
    value = ((value >> 16) & 0x0000ffff) | ((value << 16) & 0xffff0000);
    
    return value;
}

static inline uint32_t Swap(uint32_t value) {
    if (swap) {
        value = Swap_(value);
    }
    
    return value;
}

static inline uint32_t Swap(uint32_t magic, uint32_t value) {
    if (magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM) {
        value = Swap_(value);
    }
    
    return value;
}

typedef int cpu_type_t;

typedef cpu_type_t cpu_subtype_t;
typedef cpu_type_t cpu_threadtype_t;

#define CPU_ARCH_ABI64					0x01000000

#define CPU_TYPE_ANY					((cpu_type_t) -1)

#define CPU_TYPE_VAX					((cpu_type_t) 1)
#define	CPU_TYPE_MC680x0				((cpu_type_t) 6)
#define CPU_TYPE_X86					((cpu_type_t) 7)
#define CPU_TYPE_I386					(CPU_TYPE_X86)
#define	CPU_TYPE_X86_64					(CPU_TYPE_X86 | CPU_ARCH_ABI64)

#define CPU_TYPE_MC98000				((cpu_type_t) 10)
#define CPU_TYPE_HPPA					((cpu_type_t) 11)
#define CPU_TYPE_ARM					((cpu_type_t) 12)
#define CPU_TYPE_ARM64					(CPU_TYPE_ARM | CPU_ARCH_ABI64)
#define CPU_TYPE_MC88000				((cpu_type_t) 13)
#define CPU_TYPE_SPARC					((cpu_type_t) 14)
#define CPU_TYPE_I860					((cpu_type_t) 15)

#define CPU_TYPE_POWERPC				((cpu_type_t) 18)
#define CPU_TYPE_POWERPC64				(CPU_TYPE_POWERPC | CPU_ARCH_ABI64)

#define CPU_SUBTYPE_MASK				0xff000000
#define CPU_SUBTYPE_LIB64				0x80000000

#define	CPU_SUBTYPE_MULTIPLE			((cpu_subtype_t) -1)
#define CPU_SUBTYPE_LITTLE_ENDIAN		((cpu_subtype_t) 0)
#define CPU_SUBTYPE_BIG_ENDIAN			((cpu_subtype_t) 1)

#define CPU_THREADTYPE_NONE				((cpu_threadtype_t) 0)

#define	CPU_SUBTYPE_VAX_ALL				((cpu_subtype_t) 0)
#define CPU_SUBTYPE_VAX780				((cpu_subtype_t) 1)
#define CPU_SUBTYPE_VAX785				((cpu_subtype_t) 2)
#define CPU_SUBTYPE_VAX750				((cpu_subtype_t) 3)
#define CPU_SUBTYPE_VAX730				((cpu_subtype_t) 4)
#define CPU_SUBTYPE_UVAXI				((cpu_subtype_t) 5)
#define CPU_SUBTYPE_UVAXII				((cpu_subtype_t) 6)
#define CPU_SUBTYPE_VAX8200				((cpu_subtype_t) 7)
#define CPU_SUBTYPE_VAX8500				((cpu_subtype_t) 8)
#define CPU_SUBTYPE_VAX8600				((cpu_subtype_t) 9)
#define CPU_SUBTYPE_VAX8650				((cpu_subtype_t) 10)
#define CPU_SUBTYPE_VAX8800				((cpu_subtype_t) 11)
#define CPU_SUBTYPE_UVAXIII				((cpu_subtype_t) 12)

#define	CPU_SUBTYPE_MC680x0_ALL			((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MC68030				((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MC68040				((cpu_subtype_t) 2)
#define	CPU_SUBTYPE_MC68030_ONLY		((cpu_subtype_t) 3)

#define CPU_SUBTYPE_INTEL(f, m)			((cpu_subtype_t) (f) + ((m) << 4))

#define	CPU_SUBTYPE_I386_ALL			CPU_SUBTYPE_INTEL(3, 0)
#define CPU_SUBTYPE_386					CPU_SUBTYPE_INTEL(3, 0)
#define CPU_SUBTYPE_486					CPU_SUBTYPE_INTEL(4, 0)
#define CPU_SUBTYPE_486SX				CPU_SUBTYPE_INTEL(4, 8)
#define CPU_SUBTYPE_586					CPU_SUBTYPE_INTEL(5, 0)
#define CPU_SUBTYPE_PENT				CPU_SUBTYPE_INTEL(5, 0)
#define CPU_SUBTYPE_PENTPRO				CPU_SUBTYPE_INTEL(6, 1)
#define CPU_SUBTYPE_PENTII_M3			CPU_SUBTYPE_INTEL(6, 3)
#define CPU_SUBTYPE_PENTII_M5			CPU_SUBTYPE_INTEL(6, 5)
#define CPU_SUBTYPE_CELERON				CPU_SUBTYPE_INTEL(7, 6)
#define CPU_SUBTYPE_CELERON_MOBILE		CPU_SUBTYPE_INTEL(7, 7)
#define CPU_SUBTYPE_PENTIUM_3			CPU_SUBTYPE_INTEL(8, 0)
#define CPU_SUBTYPE_PENTIUM_3_M			CPU_SUBTYPE_INTEL(8, 1)
#define CPU_SUBTYPE_PENTIUM_3_XEON		CPU_SUBTYPE_INTEL(8, 2)
#define CPU_SUBTYPE_PENTIUM_M			CPU_SUBTYPE_INTEL(9, 0)
#define CPU_SUBTYPE_PENTIUM_4			CPU_SUBTYPE_INTEL(10, 0)
#define CPU_SUBTYPE_PENTIUM_4_M			CPU_SUBTYPE_INTEL(10, 1)
#define CPU_SUBTYPE_ITANIUM				CPU_SUBTYPE_INTEL(11, 0)
#define CPU_SUBTYPE_ITANIUM_2			CPU_SUBTYPE_INTEL(11, 1)
#define CPU_SUBTYPE_XEON				CPU_SUBTYPE_INTEL(12, 0)
#define CPU_SUBTYPE_XEON_MP				CPU_SUBTYPE_INTEL(12, 1)

#define CPU_SUBTYPE_INTEL_FAMILY(x)		((x) & 15)
#define CPU_SUBTYPE_INTEL_FAMILY_MAX	15

#define CPU_SUBTYPE_INTEL_MODEL(x)		((x) >> 4)
#define CPU_SUBTYPE_INTEL_MODEL_ALL		0

#define CPU_SUBTYPE_X86_ALL				((cpu_subtype_t)3)
#define CPU_SUBTYPE_X86_64_ALL			((cpu_subtype_t)3)
#define CPU_SUBTYPE_X86_ARCH1			((cpu_subtype_t)4)
#define CPU_SUBTYPE_X86_64_H			((cpu_subtype_t)8)

#define CPU_THREADTYPE_INTEL_HTT		((cpu_threadtype_t) 1)

#define	CPU_SUBTYPE_MIPS_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MIPS_R2300			((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MIPS_R2600			((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MIPS_R2800			((cpu_subtype_t) 3)
#define CPU_SUBTYPE_MIPS_R2000a			((cpu_subtype_t) 4)
#define CPU_SUBTYPE_MIPS_R2000			((cpu_subtype_t) 5)
#define CPU_SUBTYPE_MIPS_R3000a			((cpu_subtype_t) 6)
#define CPU_SUBTYPE_MIPS_R3000			((cpu_subtype_t) 7)

#define	CPU_SUBTYPE_MC98000_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MC98601				((cpu_subtype_t) 1)

#define	CPU_SUBTYPE_HPPA_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_HPPA_7100			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_HPPA_7100LC			((cpu_subtype_t) 1)

#define	CPU_SUBTYPE_MC88000_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_MC88100				((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MC88110				((cpu_subtype_t) 2)

#define	CPU_SUBTYPE_SPARC_ALL			((cpu_subtype_t) 0)

#define CPU_SUBTYPE_I860_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_I860_860			((cpu_subtype_t) 1)

#define CPU_SUBTYPE_POWERPC_ALL			((cpu_subtype_t) 0)
#define CPU_SUBTYPE_POWERPC_601			((cpu_subtype_t) 1)
#define CPU_SUBTYPE_POWERPC_602			((cpu_subtype_t) 2)
#define CPU_SUBTYPE_POWERPC_603			((cpu_subtype_t) 3)
#define CPU_SUBTYPE_POWERPC_603e		((cpu_subtype_t) 4)
#define CPU_SUBTYPE_POWERPC_603ev		((cpu_subtype_t) 5)
#define CPU_SUBTYPE_POWERPC_604			((cpu_subtype_t) 6)
#define CPU_SUBTYPE_POWERPC_604e		((cpu_subtype_t) 7)
#define CPU_SUBTYPE_POWERPC_620			((cpu_subtype_t) 8)
#define CPU_SUBTYPE_POWERPC_750			((cpu_subtype_t) 9)
#define CPU_SUBTYPE_POWERPC_7400		((cpu_subtype_t) 10)
#define CPU_SUBTYPE_POWERPC_7450		((cpu_subtype_t) 11)
#define CPU_SUBTYPE_POWERPC_970			((cpu_subtype_t) 100)

#define CPU_SUBTYPE_ARM_ALL				((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM_V4T				((cpu_subtype_t) 5)
#define CPU_SUBTYPE_ARM_V6				((cpu_subtype_t) 6)
#define CPU_SUBTYPE_ARM_V5TEJ			((cpu_subtype_t) 7)
#define CPU_SUBTYPE_ARM_XSCALE			((cpu_subtype_t) 8)
#define CPU_SUBTYPE_ARM_V7				((cpu_subtype_t) 9)
#define CPU_SUBTYPE_ARM_V7F				((cpu_subtype_t) 10)
#define CPU_SUBTYPE_ARM_V7S				((cpu_subtype_t) 11)
#define CPU_SUBTYPE_ARM_V7K				((cpu_subtype_t) 12)
#define CPU_SUBTYPE_ARM_V6M				((cpu_subtype_t) 14)
#define CPU_SUBTYPE_ARM_V7M				((cpu_subtype_t) 15)
#define CPU_SUBTYPE_ARM_V7EM			((cpu_subtype_t) 16)

#define CPU_SUBTYPE_ARM_V8				((cpu_subtype_t) 13)

#define CPU_SUBTYPE_ARM64_ALL           ((cpu_subtype_t) 0)
#define CPU_SUBTYPE_ARM64_V8            ((cpu_subtype_t) 1)

typedef struct {
    const char    *name;
    cpu_type_t    cputype;
    cpu_subtype_t cpusubtype;
    const char    *description;
} NXArchInfo;


static const NXArchInfo ArchInfoTable[] = {
    { "hppa",		CPU_TYPE_HPPA,		CPU_SUBTYPE_HPPA_ALL,		"HP-PA"},
    { "i386",		CPU_TYPE_I386,		CPU_SUBTYPE_I386_ALL,		"Intel 80x86"},
    { "x86_64",		CPU_TYPE_X86_64,	CPU_SUBTYPE_X86_64_ALL,		"Intel x86-64"},
    { "x86_64h",	CPU_TYPE_X86_64,	CPU_SUBTYPE_X86_64_H,		"Intel x86-64h Haswell"},
    { "i860",		CPU_TYPE_I860,		CPU_SUBTYPE_I860_ALL,		"Intel 860"},
    { "m68k",		CPU_TYPE_MC680x0,	CPU_SUBTYPE_MC680x0_ALL,	"Motorola 68K"},
    { "m88k",		CPU_TYPE_MC88000,	CPU_SUBTYPE_MC88000_ALL,	"Motorola 88K"},
    { "ppc",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_ALL,	"PowerPC"},
    { "ppc64",		CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_ALL,	"PowerPC 64-bit"},
    { "sparc",		CPU_TYPE_SPARC,		CPU_SUBTYPE_SPARC_ALL,		"SPARC"},
    { "arm",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_ALL,		"ARM"},
    { "arm64",		CPU_TYPE_ARM64,		CPU_SUBTYPE_ARM64_ALL,		"ARM64"},
    { "any",		CPU_TYPE_ANY,		CPU_SUBTYPE_MULTIPLE,		"Architecture Independent"},
    { "hppa7100LC", CPU_TYPE_HPPA,		CPU_SUBTYPE_HPPA_7100LC,	"HP-PA 7100LC"},
    { "m68030",		CPU_TYPE_MC680x0,	CPU_SUBTYPE_MC68030_ONLY,	"Motorola 68030"},
    { "m68040",		CPU_TYPE_MC680x0,	CPU_SUBTYPE_MC68040,		"Motorola 68040"},
    { "i486",		CPU_TYPE_I386,		CPU_SUBTYPE_486,			"Intel 80486"},
    { "i486SX",		CPU_TYPE_I386,		CPU_SUBTYPE_486SX,			"Intel 80486SX"},
    { "pentium",	CPU_TYPE_I386,		CPU_SUBTYPE_PENT,			"Intel Pentium"},
    { "i586",		CPU_TYPE_I386,		CPU_SUBTYPE_586,			"Intel 80586"},
    { "pentpro",	CPU_TYPE_I386,		CPU_SUBTYPE_PENTPRO,		"Intel Pentium Pro"},
    { "i686",		CPU_TYPE_I386,		CPU_SUBTYPE_PENTPRO,		"Intel Pentium Pro" },
    { "pentIIm3",	CPU_TYPE_I386,		CPU_SUBTYPE_PENTII_M3,		"Intel Pentium II Model 3" },
    { "pentIIm5",	CPU_TYPE_I386,		CPU_SUBTYPE_PENTII_M5,		"Intel Pentium II Model 5" },
    { "pentium4",	CPU_TYPE_I386,		CPU_SUBTYPE_PENTIUM_4,		"Intel Pentium 4" },
    { "x86_64h",	CPU_TYPE_I386,		CPU_SUBTYPE_X86_64_H,		"Intel x86-64h Haswell" },
    { "ppc601",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_601,	"PowerPC 601" },
    { "ppc603",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_603,	"PowerPC 603" },
    { "ppc603e",	CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_603e,	"PowerPC 603e" },
    { "ppc603ev",	CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_603ev,  "PowerPC 603ev" },
    { "ppc604",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_604,	"PowerPC 604" },
    { "ppc604e",	CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_604e,	"PowerPC 604e" },
    { "ppc750",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_750,	"PowerPC 750" },
    { "ppc7400",	CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_7400,	"PowerPC 7400" },
    { "ppc7450",	CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_7450,	"PowerPC 7450" },
    { "ppc970",		CPU_TYPE_POWERPC,	CPU_SUBTYPE_POWERPC_970,	"PowerPC 970" },
    { "ppc970-64",  CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_970,	"PowerPC 970 64-bit" },
    { "armv4t",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V4T,		"arm v4t" },
    { "armv5",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V5TEJ,		"arm v5" },
    { "xscale",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_XSCALE,		"arm xscale" },
    { "armv6",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V6,			"arm v6" },
    { "armv6m",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V6M,		"arm v6m" },
    { "armv7",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7,			"arm v7" },
    { "armv7f",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7F,		"arm v7f" },
    { "armv7s",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7S,		"arm v7s" },
    { "armv7k",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7K,		"arm v7k" },
    { "armv7m",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7M,		"arm v7m" },
    { "armv7em",	CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V7EM,		"arm v7em" },
    { "armv8",		CPU_TYPE_ARM,		CPU_SUBTYPE_ARM_V8,			"arm v8" },
    { "arm64",		CPU_TYPE_ARM64,		CPU_SUBTYPE_ARM64_V8,		"arm64 v8" },
    { "little",		CPU_TYPE_ANY,		CPU_SUBTYPE_LITTLE_ENDIAN,  "Little Endian" },
    { "big",		CPU_TYPE_ANY,		CPU_SUBTYPE_BIG_ENDIAN,		"Big Endian" },
    { "",			0,					0,							"" }
};

const NXArchInfo *NXGetArchInfoFromCpuType(cpu_type_t cputype, cpu_subtype_t subtype) {
    for (int i = 0; i < sizeof(ArchInfoTable); i++) {
        const NXArchInfo *archInfo = &ArchInfoTable[i];
        
        if (cputype == archInfo->cputype && subtype == archInfo->cpusubtype) {
            return archInfo;
        }
    }
    
    return &ArchInfoTable[51];
}

const NXArchInfo *NXGetArchInfoFromFatArch(struct fat_arch *arch) {
    return NXGetArchInfoFromCpuType(Swap(arch->cputype), Swap(arch->cpusubtype));
}

extern "C" CFArrayRef SBSCopyApplicationDisplayIdentifiers(bool onlyActive, bool debuggable);

extern "C" CFStringRef SBSCopyExecutablePathForDisplayIdentifier(CFStringRef bundleID);
extern "C" CFStringRef SBSCopyLocalizedApplicationNameForDisplayIdentifier(CFStringRef bundleID);

std::string CFStringGetSTDString(CFStringRef string) {
    if (!string) return "";
    
    const char *str = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);     
    if (!str) return "";
        
    return std::string(str);   
}

bool comparestr(const std::string &left, const std::string &right) {
   for (std::string::const_iterator lit = left.begin(), rit = right.begin(); lit != left.end() && rit != right.end(); ++lit, ++rit) {
       char lc = std::tolower(*lit);
       char rc = std::tolower(*rit);
       
      if (lc != rc) {
         return lc < rc;
      } 
   }
   
   return left.size() < right.size();
}

void print_installedapps() {
    CFArrayRef apps = SBSCopyApplicationDisplayIdentifiers(false, false);
    
    std::vector<std::string> applications;
                        
    for (CFIndex i = 0; i < CFArrayGetCount(apps); i++) {             
        CFStringRef bundleID = (CFStringRef)CFArrayGetValueAtIndex(apps, i);    
        std::string name = CFStringGetSTDString(SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundleID));
        
        applications.push_back(name);
    }
    
    std::sort(applications.begin(), applications.end(), comparestr); //sort alphabetically
    
    unsigned int i = 0;
    unsigned int size = applications.size();
    
    for (std::string application : applications) {
        if (!application.empty()) {
            std::cout << application;
            if (i != size) std::cout << ", ";
        }
        
        i++;
    }
    
    std::cout << std::endl;
    exit(0);
}

void print_usage() {
    std::cout << "Usage: rmaslr [-a/--app/--application]/[-b/--bin/--binary] application/binary [-arch] arch" << std::endl;
    std::cout << "Options:" << std::endl;
    
    std::cout << "    -a,     --app/--application,   Remove ASLR for an application" << std::endl;
    std::cout << "    -apps,  --applications,        Print a list of installed Applications" << std::endl;
    std::cout << "    -b,     --binary,              Remove ASLR for a Mach-O Executable" << std::endl;
    
    std::cout << "    -?/-h,  --help,                Print this message" << std::endl;
    exit(0);
}

int main(int argc, const char * argv[]) {
    if (argc < 2) {
        print_usage();
    }
    
    bool appFlag = false;
    bool binFlag = false;
    
    const char *arg1 = argv[1];
    switch (arg1[0]) {
        case '-': {
            if (strcasecmp(arg1, "-apps") == 0 || strcasecmp(arg1, "--applications") == 0) {
                print_installedapps();
            } else if (strcasecmp(arg1, "-?") == 0 || strcasecmp(arg1, "-h") == 0 || strcasecmp(arg1, "--help") == 0) {
                print_usage();
            } if (strcasecmp(arg1, "-a") == 0 || strcasecmp(arg1, "--app") == 0 || strcasecmp(arg1, "--application") == 0) {
                appFlag = true;
            } else if (strcasecmp(arg1, "-b") == 0 || strcasecmp(arg1, "--bin") == 0 || strcasecmp(arg1, "--binary") == 0) {
                binFlag = true;
            } else {
                error("Option " << arg1 << " not found");
            }
            
            if (arg1[1] == '\0') {
                error("Please provide an Option");
            }
            
            break;
        }
        
        default:
            error("Please provide an Option");
            break;
    }
    
    FILE *file = NULL;
    
    //perform validations
    if (binFlag) {
        if (argc < 3) {
            error("Option " << arg1 << " requires a path to a mach-o executable");
        }
        
        std::string path = argv[2];
        if (path[0] != '/') {
            char buf[4096];
            
            if (!getcwd(buf, sizeof(buf))) {
                error("getcwd() did not return a valid path");
            }
            
            size_t length = strlen(buf);
            
            if (buf[length - 1] != '/') {
                buf[length] = '/';
                buf[length + 1] = '\0'; 
            }
            
            path.insert(0, buf);
        }
        
        if (access(path.c_str(), W_OK) != 0) {
            if (access(path.c_str(), F_OK) != 0) {
                error("Invalid Path provided - " << path);
            } else if (access(path.c_str(), R_OK) != 0) {
                error("Unable to access write permissions for file at path, Please change permissions of the file");
            }
        }
        
        struct stat *sbuf = new struct stat;
        stat(path.c_str(), sbuf);
        
        if (S_ISDIR(sbuf->st_mode)) {
            error("Directories are not supported");
        } else if (!S_ISREG(sbuf->st_mode)) {
            error("Only Mach-O Executables are supported");
        }
            
        if (sbuf->st_size < sizeof(struct mach_header)) {
            error("Size of file is too small");
        }   
         
        file = fopen(path.c_str(), "r+");
        if (!file) {
            error("Unable to open file at path, Please change permissions of the file");
        }
    } else {
        if (argc < 3) {
            assert_("Option " << arg1 << " requires an application name/identifier/executable-name");
        }
        
        std::string app = argv[2]; //accepted options are localized-name, binary-name, bundleID
        
        //look for applications
        bool found = false;
        CFArrayRef apps = SBSCopyApplicationDisplayIdentifiers(false, false);
        
        std::string bundleID;
        std::string name;
        std::string bin; //binary name
        
        std::string executablepath;
         
        for (CFIndex i = 0; i < CFArrayGetCount(apps); i++) {             
            CFStringRef bundleID_ = (CFStringRef)CFArrayGetValueAtIndex(apps, i);
            bundleID = CFStringGetSTDString(bundleID_);
            
            name = CFStringGetSTDString(SBSCopyLocalizedApplicationNameForDisplayIdentifier(bundleID_));
            bin = CFStringGetSTDString(SBSCopyExecutablePathForDisplayIdentifier(bundleID_));
            
            executablepath = bin; 
                       
            std::string::size_type index = bin.find_last_of('/');
            bin = bin.substr(index + 1);
            
            if (strcasecmp(bundleID.c_str(), app.c_str()) == 0 || strcasecmp(name.c_str(), app.c_str()) == 0 || strcasecmp(bin.c_str(), app.c_str()) == 0) {
                found = true;
                break;
            }
        }
                
        if (!found) {
            error("Application " << app << " was not found. Run -apps/--applications to see a list of applications");
        }
        
        file = fopen(executablepath.c_str(), "r+");
        if (!file) {
            error("Unable to open file at path");
        }
    }
    
    uint32_t magic;
    
    fseek(file, 0x0, SEEK_SET);
    fread(&magic, sizeof(uint32_t), 1, file);
    
    bool fat = false;
    bool bits64 = false;
    
    switch (magic) {
        case MH_MAGIC:
            break;
        case MH_CIGAM:
            swap = true;
            break;
        case MH_CIGAM_64:
            swap = true;
        case MH_MAGIC_64:
            bits64 = true;
            break;
        case FAT_CIGAM:
            swap = true;
        case FAT_MAGIC:
            fat = true;
            break;
        default:
            error("File at path is not a mach-o executable");
            break;
    }
    
    if (!fat && argc > 3) {
        if (strcasecmp(argv[3], "-arch") == 0) {
            error("File at path does not contain multiple architectures");
        } else {
            error("Too many arguments provided");
        }
    } else if (fat && argc > 5) {
        error("Too many arguments provided");
    }
    
    std::map<uint32_t, struct mach_header *> headers;
    
    if (fat) {
        struct fat_header *fat = new struct fat_header;
        
        std::vector<struct fat_arch *> archs;
        std::vector<const NXArchInfo *> archInfos;
        
        fseek(file, 0x0, SEEK_SET);
        fread(fat, sizeof(struct fat_header), 1, file);
        
        uint32_t nfat_arch = Swap(fat->nfat_arch);
        uint32_t off = sizeof(struct fat_header);
        
        for (uint32_t i = 0; i < nfat_arch; i++) {
            struct fat_arch *arch = new struct fat_arch;
            
            fseek(file, off, SEEK_SET);
            fread(arch, sizeof(struct fat_arch), 1, file);
            
            archs.push_back(arch);
            archInfos.push_back(NXGetArchInfoFromFatArch(arch));
            
            off += sizeof(struct fat_arch);
        }
        
        std::vector<struct fat_arch *> archs_; //selected architectures
        
        if (argc < 5) {
            std::cout << "Fat Mach-O executable detected\nPlease choose one of the following architectures: [" << archInfos[0]->name;
            
            std::vector<const NXArchInfo *> archInfos_ = archInfos;
            archInfos_.erase(archInfos_.begin());
            
            for (const NXArchInfo *archInfo : archInfos_) {
                std::cout << ", " << archInfo->name;
            }
            
            std::cout << "]\nOr enter \'all\' for all architectures: ";
            
            while (true) {
                std::string arch_;
                std::cin >> arch_;
                
                if (strcasecmp(arch_.c_str(), "all") == 0) {
                    archs_ = archs;
                    break;
                } else {
                    bool found = false;
                    
                    int i = 0;
                    for (const NXArchInfo *archInfo : archInfos) {
                        if (strcasecmp(archInfo->name, arch_.c_str()) != 0) {
                            i++; 
                            continue;
                        }
                        
                        archs_.push_back(archs[i]);
                        found = true;
                        
                        break;
                    }
                 
                    if (!found) {
                        std::cout << "Architecture " << arch_ << " not found, Please choose an architecture from the list above";
                    } else {
                        break;
                    }
                }
            }
        } else {
            if (strcasecmp(argv[3], "-arch") != 0) {
                error("Flag " << argv[3] << " not recognized, Did you mean -arch?");
            }
            
            std::string arch = argv[4];
            if (strcasecmp(arch.c_str(), "all") == 0) {
                archs_ = archs;
            } else {
                bool found = false;
                for (uint32_t i = 0; i < nfat_arch; i++) {
                    if (strcasecmp(archInfos[i]->name, arch.c_str()) != 0) continue;
                    
                    archs_.push_back(archs[i]);
                    found = true;
                    
                    break;
                }
                
                if (!found) {
                    error("Unable to find Architecture \'" << arch << "\' Please choose an architecture from the list above");
                }
            }   
        }
        
        for (struct fat_arch *arch : archs_) {
            struct mach_header *header = new struct mach_header;
            uint32_t offset = Swap(arch->offset);
                        
            fseek(file, offset, SEEK_SET);
            fread(header, sizeof(struct mach_header), 1, file);
            
            headers.insert(std::pair<uint32_t, struct mach_header *>(offset, header));
        }
    } else {
        if (argc > 3) {
            if (strcasecmp(argv[3], "-arch") != 0) {
                error("Too many arguments provided");
            } else {
                error("Application does not contain multiple architectures");
            }
        }
    }

    for (auto const &it : headers) {
        uint32_t offset = it.first;
        struct mach_header *header = it.second;
                
        if (!(Swap(header->magic, header->flags) & MH_PIE)) {
            if (fat) {
                std::cout << "Architecture " << NXGetArchInfoFromCpuType(Swap(header->magic, header->cputype), Swap(header->magic, header->cpusubtype))->name << " does not contain ASLR" << std::endl;
            } else {
                std::cout << "Mach-O Executable does not contain ASLR" << std::endl;
            }
            
            continue;
        }
        
        uint32_t flags = Swap(header->magic, header->flags);
        flags &= ~MH_PIE;
        
        if (header->magic == MH_CIGAM || header->magic == MH_CIGAM_64) {
            flags = Swap_(flags); //swap back
        }
        
        header->flags = flags;
        
        fseek(file, offset, SEEK_SET);
        size_t err = fwrite(header, sizeof(struct mach_header), 1, file);
        
        if (err != 1) {
            if (fat) {
                error("Failed to write header to Architecture " << NXGetArchInfoFromCpuType(Swap(header->magic, header->cputype), Swap(header->magic, header->cpusubtype))->name);
            } else {
                error("Failed to write header to File");
            }
        }
        
        if (fat) 
            std::cout << "Architecture " << NXGetArchInfoFromCpuType(Swap(header->magic, header->cputype), Swap(header->magic, header->cpusubtype))->name << " no longer contains ASLR" << std::endl;
        else 
            std::cout << "Mach-O Executable no longer contains ASLR" << std::endl;
    }
    
    return 0;
}