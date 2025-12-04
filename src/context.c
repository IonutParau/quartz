#include "context.h"
#include "value.h"
#include "platform.h"

#ifndef QUARTZ_NO_LIBC
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#endif

#ifdef QUARTZ_POSIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

size_t quartz_sizeOfContext() {
	return sizeof(quartz_Context);
}

static void *quartzI_defaultAlloc(void *userdata, void *memory, size_t oldSize, size_t newSize) {
#ifdef QUARTZ_NO_LIBC
	return NULL; // OOM
#else
	if(newSize == 0) {
		free(memory);
		return NULL;
	}
	return realloc(memory, newSize);
#endif
}

static quartz_Errno quartzI_defaultFile(quartz_Thread *Q, void *userdata, void **fileData, quartz_FsAction action, void *buf, size_t *buflen) {
#ifdef QUARTZ_NO_LIBC
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "filesystem is not defined, I/O is limited");
#else
	if(action == QUARTZ_FS_STDFILE) {
		quartz_StdFile s = *(quartz_StdFile *)buf;
		FILE *files[QUARTZ_STDFILE_COUNT] = {
			[QUARTZ_STDIN] = stdin,
			[QUARTZ_STDOUT] = stdout,
			[QUARTZ_STDERR] = stderr,
		};
		*fileData = files[s];
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_OPEN) {
		quartz_FileMode fmode = *(quartz_FileMode *)(void *)buflen;
		char mode[3] = "r";
		mode[1] = '\0';
		mode[2] = '\0';

		if(fmode & QUARTZ_FILE_READABLE) mode[0] = 'r';
		if(fmode & QUARTZ_FILE_WRITABLE) mode[0] = 'w';
		if(fmode & QUARTZ_FILE_APPEND) mode[0] = 'a';
		if(fmode & QUARTZ_FILE_BINARY) mode[1] = 'b';
		
		FILE *f = fopen(buf, mode);
		if(f == NULL) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
		}
		setvbuf(f, NULL, _IONBF, 0);
		*(FILE **)fileData = f;
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_CLOSE) {
		FILE *f = *(FILE **)fileData;
		if(fclose(f) != 0) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
		}
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_READ) {
		FILE *f = *(FILE **)fileData;
		size_t written = 0;
		size_t toRead = *buflen;
		while(written < toRead) {
			if(feof(f)) break;
			written += fread(buf + written, 1, toRead - written, f);
			int e = ferror(f);
			if(e) {
				return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(e));
			}
		}
		*buflen = written;
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_WRITE) {
		FILE *f = *(FILE **)fileData;
		*buflen = fwrite(buf, 1, *buflen, f);
		if(fflush(f)) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
		}
		// we could check by length but doing so is weird
		return QUARTZ_OK;
	}
	if(action == QUARTZ_FS_SEEK) {
		FILE *f = *(FILE **)fileData;
		quartz_FsSeekArg arg = *(quartz_FsSeekArg *)buf;
		int whence = SEEK_SET;
		if(arg.whence == QUARTZ_SEEK_SET) whence = SEEK_SET;
		if(arg.whence == QUARTZ_SEEK_CUR) whence = SEEK_CUR;
		if(arg.whence == QUARTZ_SEEK_END) whence = SEEK_END;
		if(fseek(f, arg.off, whence)) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
		}
		*buflen = ftell(f);
		// we could check by length but doing so is weird
		return QUARTZ_OK;
	}
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "filesystem is action is unsupported, something is horribly wrong");
#endif
}

#ifdef QUARTZ_LINUX
#define QUARTZ_PATHPREFIX "/lib/@.qrtz;/lib/@/lib.qrtz;/usr/lib/@.qrtz;/usr/lib/@.qrtz;"
#define QUARTZ_CPATHPREFIX "/lib/@.so;/usr/lib/quartz/@.so;/usr/lib/local/quartz/@.so;"
#else
#define QUARTZ_PATHPREFIX ""
#define QUARTZ_CPATHPREFIX ""
#endif

static const char *quartzI_defaultPath() {
	return QUARTZ_PATHPREFIX "@.qrtz;@/lib.qrtz;lib/@.qrtz;lib/@/lib.qrtz";
}

static const char *quartzI_defaultCPath() {
	return QUARTZ_CPATHPREFIX "@.so;lib/@.so;loadall.so";
}

static void quartzI_defaultLog(void *context, const char *msg, size_t msglen) {
#ifndef QUARTZ_NO_LIBC
	fwrite(msg, sizeof(char), msglen, stderr);
#endif
}

static quartz_Errno quartzI_defaultOs(quartz_Thread *Q, void *context, quartz_OsFunc func, quartz_OsArgs *const args, quartz_OsRet *ret) {
#ifndef QUARTZ_NO_LIBC
	if(func == QUARTZ_OSF_SPAWN) {
#ifdef QUARTZ_POSIX
	pid_t pid = fork();
	if(pid == -1) {
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
	}
	if(pid == 0) {
		// we're the new process!
		const char * const *env = args->spawn.env;
		int err;
		if(env == NULL) {
			err = execv(args->spawn.path, (char * const *)args->spawn.argv);
		} else {
			err = execve(args->spawn.path, (char * const *)args->spawn.argv, (char * const *) env);
		}
		exit(EXIT_FAILURE); // we're meant to be dead!
	} else {
		while(1) {
			int status = 0;
			if(waitpid(pid, &status, 0) == -1) {
				return quartz_errorf(Q, QUARTZ_ERUNTIME, "%s", strerror(errno));
			}
			if(WIFSIGNALED(status)) {
				ret->spawnOrExec.exited = false;
				ret->spawnOrExec.signal = WTERMSIG(status);
				return QUARTZ_OK;
			}
			if(WIFEXITED(status)) {
				ret->spawnOrExec.exited = true;
				ret->spawnOrExec.exitcode = WEXITSTATUS(status);
				return QUARTZ_OK;
			}
		}
	}
#elif defined(QUARTZ_WINDOWS)
#endif
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "unsupported function");
	}
	if(func == QUARTZ_OSF_EXEC) {
		ret->spawnOrExec.exited = true;
		ret->spawnOrExec.exitcode = system(args->exec.cmd);
		return QUARTZ_OK;
	}
#endif
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "unsupported function");
}

void quartz_initContext(quartz_Context *ctx, void *userdata) {
	ctx->userdata = userdata;
	ctx->alloc = quartzI_defaultAlloc;
	ctx->fs = quartzI_defaultFile;
#ifdef QUARTZ_NO_LIBC
	ctx->fsDefaultBufSize = 1024;
#else
	ctx->fsDefaultBufSize = BUFSIZ;
#endif
	ctx->logflags = 0;
	ctx->logf = quartzI_defaultLog;
	ctx->osf = quartzI_defaultOs;
	quartz_setModuleConfig(ctx, NULL, NULL, NULL);
}

void *quartz_rawAlloc(quartz_Context *ctx, size_t size) {
	if(size == 0) return ctx->alloc;
	return ctx->alloc(ctx->userdata, NULL, 0, size);
}

void *quartz_rawRealloc(quartz_Context *ctx, void *memory, size_t oldSize, size_t newSize) {
	if(memory == NULL || memory == ctx->alloc) {
		return quartz_rawAlloc(ctx, newSize);
	}
	return ctx->alloc(ctx->userdata, memory, oldSize, newSize);
}

void quartz_rawFree(quartz_Context *ctx, void *memory, size_t size) {
	if(memory == ctx->alloc) return;
	ctx->alloc(ctx->userdata, memory, size, 0);
}

double quartz_rawClock(quartz_Context *ctx) {
	return ctx->clock(ctx->userdata);
}

size_t quartz_rawTime(quartz_Context *ctx) {
	return ctx->time(ctx->userdata);
}

quartz_Errno quartz_osfunc(quartz_Thread *Q, quartz_OsFunc func, quartz_OsArgs *const args, quartz_OsRet *ret) {
	quartz_Context ctx = Q->gState->context;
	return ctx.osf(Q, ctx.userdata, func, args, ret);
}

void quartz_setAllocator(quartz_Context *ctx, quartz_Allocf alloc) {
	ctx->alloc = alloc;
}

void quartz_setClock(quartz_Context *ctx, quartz_Clockf clock) {
	ctx->clock = clock;
}

void quartz_setTime(quartz_Context *ctx, quartz_Timef time) {
	ctx->time = time;
}

void quartz_setFileSystem(quartz_Context *ctx, quartz_Filef file, size_t defaultFileBufSize) {
	ctx->fs = file;
	ctx->fsDefaultBufSize = defaultFileBufSize;
}

void quartz_setLogging(quartz_Context *ctx, quartz_LogFlags flags, quartz_Logf f) {
	ctx->logflags = flags;
	if(f != NULL) ctx->logf = f;
}

void quartz_setModuleConfig(quartz_Context *ctx, const char *path, const char *cpath, const char *pathConfig) {
	ctx->modPathConf[QUARTZ_PATHCONF_PATHSEP] = QUARTZ_PATHSEPC;
	ctx->modPathConf[QUARTZ_PATHCONF_SYMSEP] = '_';
	ctx->modPathConf[QUARTZ_PATHCONF_COUNT] = '\0';

	if(path == NULL) path = quartzI_defaultPath();
	if(cpath == NULL) cpath = quartzI_defaultCPath();

	ctx->modPath = path;
	ctx->modCPath = cpath;

	if(pathConfig) {
		for(size_t i = 0; i < QUARTZ_PATHCONF_COUNT; i++) {
			if(pathConfig[i]) break;
			ctx->modPathConf[i] = pathConfig[i];
		}
	}
}

const char *quartz_getModulePath(const quartz_Context *ctx) {
	return ctx->modPath;
}

const char *quartz_getModuleCPath(const quartz_Context *ctx) {
	return ctx->modCPath;
}

const char *quartz_getModulePathConf(const quartz_Context *ctx) {
	return ctx->modPathConf;
}

quartz_Context *quartz_getContextOf(quartz_Thread *Q) {
	return &Q->gState->context;
}
