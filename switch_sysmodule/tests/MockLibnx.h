#ifndef MOCK_LIBNX_H
#define MOCK_LIBNX_H

#include <stdint.h>
#include <string.h>
#include <string>
#include <map>

// Mocking Switch types
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint8_t u8;
#define CUR_PROCESS_HANDLE 0

// Mocking filesystem result code
typedef int Result;
#define R_SUCCEEDED(res) ((res) == 0)
#define R_FAILED(res) ((res) != 0)

// Mocking libnx functions used by ConfigManager
inline void mkdir(const char* path, int mode) {}

// Mocking some NIFM/Setsys for testing structure
inline Result nifmGetCurrentIpAddress(u32* ip) { *ip = 0x0100007f; return 0; }
inline u64 svcGetSystemTick() { return 123456789; }

#endif
