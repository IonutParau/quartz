#ifndef QUARTZ_CTX_H
#define QUARTZ_CTX_H

#include "quartz.h"

typedef enum quartz_PathConfIdx {
	QUARTZ_PATHCONF_PATHSEP = 0,
	QUARTZ_PATHCONF_SYMSEP,
	QUARTZ_PATHCONF_COUNT,
	QUARTZ_PATHCONF_SIZE,
} quartz_PathConfIdx;

typedef struct quartz_Context {
	void *userdata;
	quartz_Allocf alloc;
	quartz_Clockf clock;
	quartz_Timef time;
	quartz_Filef fs;
	size_t fsDefaultBufSize;
	quartz_LogFlags logflags;
	quartz_Logf logf;
	quartz_Osf osf;
	const char *modPath;
	const char *modCPath;
	char modPathConf[QUARTZ_PATHCONF_SIZE];
} quartz_Context;

void *quartz_rawAlloc(quartz_Context *ctx, size_t size);
void *quartz_rawRealloc(quartz_Context *ctx, void *memory, size_t oldSize, size_t newSize);
void quartz_rawFree(quartz_Context *ctx, void *memory, size_t size);

double quartz_rawClock(quartz_Context *ctx);
size_t quartz_rawTime(quartz_Context *ctx);

#endif
