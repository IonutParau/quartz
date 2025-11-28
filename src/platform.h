#ifndef QUARTZ_PLATFORM_H
#define QUARTZ_PLATFORM_H

// Based off https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	//define something for Windows (32-bit and 64-bit, this part is common)
	#define QUARTZ_WINDOWS
#elif __APPLE__
	#include <TargetConditionals.h>
    #define QUARTZ_OSX
    #if TARGET_IPHONE_SIMULATOR
		#define QUARTZ_IOS
    #elif TARGET_OS_MACCATALYST
        // I guess?
        #define QUARTZ_MACOS
    #elif TARGET_OS_IPHONE
		#define QUARTZ_IOS
    #elif TARGET_OS_MAC
        #define QUARTZ_MACOS
    #endif
#elif __ANDROID__
	#define QUARTZ_ANDROID
#elif __linux__
    #define QUARTZ_LINUX
// TODO: WASI support
#else
	// unknown OS, we assume embedded
	#define QUARTZ_EMBEDDED
#endif

#if __unix__ // all unices not caught above
    // Unix
    #define QUARTZ_UNIX
    #define QUARTZ_POSIX
#elif defined(_POSIX_VERSION)
    // POSIX
    #define QUARTZ_POSIX
#endif

#if defined(QUARTZ_WINDOWS)
	#define QUARTZ_PATHSEPC '\\'
	#define QUARTZ_PATHSEP "\\"
#else
	#define QUARTZ_PATHSEPC '/'
	#define QUARTZ_PATHSEP "/"
#endif

#ifndef QUARTZ_BITSPERUNIT
#ifdef __CHAR_BIT__
	#define QUARTZ_BITSPERUNIT (__CHAR_BIT__ / sizeof(char))
#else
	// assume bytes
	#define QUARTZ_BITSPERUNIT 8
#endif
#endif

#endif
