# rmaslr
Command line tool to Remove ASLR from mach-o binaries and ios/mac Applications
```
Usage: rmaslr -a application
Options:
	-a,     --app/--application,   Remove ASLR for an application
	-apps,  --applications,        Print a list of Applications
	-arch,  --architecture,        Single out an architecture to remove ASLR from
	-archs, --architectures,       Print all possible architectures, and if application/binary is provided, print all architectures present
	-b,     --binary,              Remove ASLR for a Mach-O Executable
	-c,     --check,               Check if application or binary contains ASLR
    -h,     --help,                Print this message
    -u,     --usage,               Print this message
```
