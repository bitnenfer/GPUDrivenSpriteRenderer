#pragma once

#define WIN32_LEAN_AND_MEAN 1

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <Windows.h>
#undef max

#define DEBUG_BREAK() __debugbreak()
#define LOG(fmt, ...) logFmt(fmt "\n", ##__VA_ARGS__)
#define PANIC(fmt, ...)                                                     \
    logFmt("PANIC: " fmt "\n", ##__VA_ARGS__);                      \
    DEBUG_BREAK();                                                          \
    ExitProcess(~0);
#define ASSERT(x, fmt, ...)                                                 \
    if (!(x)) {                                                                \
        logFmt("ASSERT: " fmt "\n", ##__VA_ARGS__);                 \
        DEBUG_BREAK();                                                      \
    }

void logFmt(const char* fmt, ...);
uint64_t murmurHash(const void* key, uint64_t keyLength, uint64_t seed);
inline void* offsetPtr(void* Ptr, intptr_t Offset) {
    return (void*)((intptr_t)Ptr + Offset);
}
inline void* alignPtr(void* Ptr, size_t Alignment) {
    return (void*)(((uintptr_t)(Ptr)+((uintptr_t)(Alignment)-1LL)) &
        ~((uintptr_t)(Alignment)-1LL));
}
inline size_t alignSize(size_t Value, size_t Alignment) {
    return ((Value)+((Alignment)-1LL)) & ~((Alignment)-1LL);
}

size_t getFileSize(const char* path);
bool readFile(const char* path, void* outBuffer);
void* allocReadFile(const char* path);

struct FileReader {
    FileReader(const char* path) {
        size = getFileSize(path);
        buffer = allocReadFile(path);
    }
    ~FileReader() {
        free(buffer);
    }
    void* operator*() {
        return buffer;
    }
    size_t getSize() const {
        return size;
    }
private:
    void* buffer = nullptr;
    size_t size = 0;
};