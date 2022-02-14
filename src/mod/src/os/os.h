#pragma once

#include <gctypes.h>

#define PANIC(...) OSPanic(__FILE__, __LINE__, __VA_ARGS__)

extern "C" {

extern struct {
	u32 game;
	u16 company;
} __GameCode;

[[noreturn]] void OSPanic(const char *file, int line, const char *fmt, ...);
void OSReport(const char *fmt, ...);
u64 OSGetTime();

bool OSDisableInterrupts();
void OSRestoreInterrupts(bool level);

}