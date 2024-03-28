#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <Windows.h>

#define NK_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE  4096
#define NK_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT 4

void logFmt(const char* fmt, ...) {
    static char bufferLarge[NK_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE *
        NK_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT] = {};
    static uint32_t bufferIndex = 0;
    va_list args;
    va_start(args, fmt);
    char* buffer =
        &bufferLarge[bufferIndex * NK_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE];
    vsprintf_s(buffer, NK_UTILS_WINDOWS_LOG_MAX_BUFFER_SIZE, fmt, args);
    bufferIndex = (bufferIndex + 1) % NK_UTILS_WINDOWS_LOG_MAX_BUFFER_COUNT;
    va_end(args);
    OutputDebugStringA(buffer);
    printf("%s", buffer);
}

// Origin: https://github.com/niklas-ourmachinery/bitsquid-foundation/blob/master/murmur_hash.cpp
uint64_t murmurHash(const void* key, uint64_t keyLength, uint64_t seed) {
		const uint64_t m = 0xc6a4a7935bd1e995ULL;
		const uint32_t r = 47;
		uint64_t h = seed ^ (keyLength * m);
		const uint64_t* data = (const uint64_t*)key;
		const uint64_t* end = data + (keyLength / 8);
		while (data != end) {
			uint64_t k = *data++;
			k *= m;
			k ^= k >> r;
			k *= m;
			h ^= k;
			h *= m;
		}
		const unsigned char* data2 = (const unsigned char*)data;
		switch (keyLength & 7) {
		case 7: h ^= uint64_t(data2[6]) << 48;
		case 6: h ^= uint64_t(data2[5]) << 40;
		case 5: h ^= uint64_t(data2[4]) << 32;
		case 4: h ^= uint64_t(data2[3]) << 24;
		case 3: h ^= uint64_t(data2[2]) << 16;
		case 2: h ^= uint64_t(data2[1]) << 8;
		case 1: h ^= uint64_t(data2[0]);
			h *= m;
		};
		h ^= h >> r;
		h *= m;
		h ^= h >> r;
		return h;
}

size_t getFileSize(const char* path) {
	HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fileSize = {};
		if (GetFileSizeEx(fileHandle, &fileSize)) {
			return fileSize.QuadPart;
		}
		CloseHandle(fileHandle);
	}
	return 0;
}
bool readFile(const char* path, void* outBuffer) {
	HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fileSize = {};
		GetFileSizeEx(fileHandle, &fileSize);
		DWORD readBytes = 0;
		if (!ReadFile(fileHandle, outBuffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
			CloseHandle(fileHandle);
			return false;
		}
		CloseHandle(fileHandle);
		return true;
	}
	return false;
}
void* allocReadFile(const char* path) {
	HANDLE fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fileSize = {};
		GetFileSizeEx(fileHandle, &fileSize);
		void* buffer = malloc(fileSize.QuadPart);
		DWORD readBytes = 0;
		if (!ReadFile(fileHandle, buffer, (DWORD)fileSize.QuadPart, &readBytes, nullptr)) {
			CloseHandle(fileHandle);
			free(buffer);
			return nullptr;
		}
		CloseHandle(fileHandle);
		return buffer;
	}
	return nullptr;
}