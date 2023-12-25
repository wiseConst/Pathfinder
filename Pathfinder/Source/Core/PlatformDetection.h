#ifndef PLATFORMDETECTION_H
#define PLATFORMDETECTION_H

// GCC predefined compiler macros

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <Windows.h>
#define PFR_WINDOWS 1
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#define PFR_MACOS 1
#if TARGET_IPHONE_SIMULATOR
// iOS, tvOS, or watchOS Simulator
#elif TARGET_OS_MACCATALYST
// Mac's Catalyst (ports iOS API into Mac, like UIKit).
#elif TARGET_OS_IPHONE
// iOS, tvOS, or watchOS device
#elif TARGET_OS_MAC
// Other kinds of Apple platforms
#else
#error "Unknown Apple platform"
#endif
#elif defined(__ANDOIRD__)
#error "Andoird not supported!"
#elif defined(__linux__)
#define PFR_LINUX 1
#elif defined(__unix__)
#error "Unix not supported!"
#elif defined(_POSIX_VERSION)
#error "Posix not supported!"
#endif

#endif  // PLATFORMDETECTION_H
