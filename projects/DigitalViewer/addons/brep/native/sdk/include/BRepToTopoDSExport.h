#pragma once

#if defined(_WIN32)
#if defined(BREP_TOPODS_IMPLEMENTATION)
#define BREP_TOPODS_EXPORT __declspec(dllexport)
#else
#define BREP_TOPODS_EXPORT __declspec(dllimport)
#endif
#elif defined(__linux__)
#if defined(BREP_TOPODS_IMPLEMENTATION)
#define BREP_TOPODS_EXPORT __attribute__((visibility("default")))
#else
#define BREP_TOPODS_EXPORT
#endif
#else
#error "Unsupported platform"
#endif
