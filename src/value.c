#include "quartz.h"
#include "common.h"
#include "value.h"
#include <math.h>

void *qrtz_alloc(qrtz_VM *vm, size_t len) {
	vm->memUsage += len;
	return qrtz_calloc(&vm->ctx, len);
}

void *qrtz_allocArray(qrtz_VM *vm, size_t len, size_t count) {
	vm->memUsage += len * count;
	return qrtz_callocArray(&vm->ctx, len, count);
}

void qrtz_free(qrtz_VM *vm, void *memory, size_t len) {
	vm->memUsage -= len;
	qrtz_cfree(&vm->ctx, memory, len);
}

void qrtz_freeArray(qrtz_VM *vm, void *memory, size_t len, size_t count) {
	vm->memUsage -= len * count;
	qrtz_cfreeArray(&vm->ctx, memory, len, count);
}

void *qrtz_realloc(qrtz_VM *vm, void *memory, size_t len, size_t oldCount, size_t newCount) {
	vm->memUsage -= len * oldCount;
	vm->memUsage += len * newCount;
	return qrtz_crealloc(&vm->ctx, memory, len, oldCount, newCount);
}

qrtz_Object *qrtz_allocObject(qrtz_VM *vm, qrtz_ObjTag tag, size_t objSize) {
	qrtz_Object *o = qrtz_alloc(vm, objSize);
	if(o == NULL) return NULL;
	o->next = vm->heap;
	o->nextGray = NULL;
	o->tag = tag;
	o->marked = false;
	vm->heap = o;
	return o;
}

qrtz_String *qrtz_allocStringObject(qrtz_VM *vm, const char *data, size_t len) {
	qrtz_String *s = (qrtz_String *)qrtz_allocObject(vm, QRTZ_OSTR, sizeof(qrtz_String) + len + 1);
	if(s == NULL) return NULL;
	s->len = len;
	if(data != NULL) {
		qrtz_memcpy(s->data, data, len);
		s->hash = qrtz_strhash(data, len);
	}
	s->data[len] = '\0';
	return s;
}

qrtz_Array *qrtz_allocArrayObject(qrtz_VM *vm, size_t cap) {
	qrtz_Array *arr = (qrtz_Array *)qrtz_allocObject(vm, QRTZ_OARRAY, sizeof(qrtz_Array) + sizeof(qrtz_Value) * cap);
	if(arr == NULL) return NULL;
	arr->len = 0;
	arr->cap = cap;
	return arr;
}

qrtz_Map *qrtz_allocMapObject(qrtz_VM *vm, size_t cap) {
	if(qrtz_sizeOverflows(cap, 2)) return NULL;
	qrtz_Value *backing = qrtz_allocArray(vm, sizeof(qrtz_Value), cap * 2);
	if(backing == NULL) return NULL;
	qrtz_Map *m = (qrtz_Map *)qrtz_allocObject(vm, QRTZ_OMAP, sizeof(qrtz_Map));
	if(m == NULL) {
		qrtz_freeArray(vm, backing, sizeof(qrtz_Value), cap * 2);
		return NULL;
	}
	m->used = 0;
	m->len = 0;
	m->cap = cap;
	m->data = backing;
	for(size_t i = 0; i < cap; i++) {
		// mark keys as unallocated
		m->data[i].tag = QRTZ_VILLEGAL;
	}
	return m;
}

qrtz_Pointer *qrtz_allocPointerObject(qrtz_VM *vm) {
	qrtz_Pointer *p = (qrtz_Pointer *)qrtz_allocObject(vm, QRTZ_OPOINTER, sizeof(qrtz_Pointer));
	if(p == NULL) return NULL;
	p->val.tag = QRTZ_VNULL;
	return p;
}

qrtz_Task *qrtz_allocTaskObject(qrtz_VM *vm, qrtz_Map *globals) {
	size_t initialStack = 32, initialCall = 8;
	qrtz_Value *stack = qrtz_allocArray(vm, sizeof(qrtz_Value), initialStack);
	if(stack == NULL) return NULL;
	qrtz_CallEntry *call = qrtz_allocArray(vm, sizeof(qrtz_CallEntry), initialCall);
	if(call == NULL) {
		qrtz_freeArray(vm, stack, sizeof(qrtz_Value), initialStack);
		return NULL;
	}
	qrtz_Task *t = (qrtz_Task *)qrtz_allocObject(vm, QRTZ_OTASK, sizeof(qrtz_Task));
	if(t == NULL) {
		qrtz_freeArray(vm, call, sizeof(qrtz_CallEntry), initialCall);
		qrtz_freeArray(vm, stack, sizeof(qrtz_Value), initialStack);
		return NULL;
	}
	t->stacklen = 0;
	t->stackcap = initialStack;
	t->calllen = 0;
	t->callcap = initialCall;
	t->stack = stack;
	t->calls = call;
	t->waitingFor = NULL;
	t->waitedBy = NULL;
	t->deadline = 0;
	t->checkcounter = 0;
	t->checkinterval = 0;
	t->error.tag = QRTZ_VILLEGAL;
	return t;
}

static void qrtz_objfree(qrtz_VM *vm, qrtz_Object *obj) {
	if(obj->tag == QRTZ_OSTR) {
		qrtz_String *s = (qrtz_String *)obj;
		qrtz_free(vm, s, sizeof(qrtz_String) + s->len + 1);
		return;
	}
	if(obj->tag == QRTZ_OARRAY) {
		qrtz_Array *arr = (qrtz_Array *)obj;
		qrtz_free(vm, arr, sizeof(qrtz_Array) + sizeof(qrtz_Array) * arr->cap);
		return;
	}
	if(obj->tag == QRTZ_OMAP) {
		qrtz_Map *map = (qrtz_Map *)obj;
		qrtz_freeArray(vm, map->data, sizeof(qrtz_Value), map->cap * 2);
		qrtz_free(vm, map, sizeof(qrtz_Map));
		return;
	}
	if(obj->tag == QRTZ_OPOINTER) {
		qrtz_Pointer *ptr = (qrtz_Pointer *)obj;
		qrtz_free(vm, ptr, sizeof(qrtz_Pointer));
		return;
	}
	if(obj->tag == QRTZ_OTASK) {
		qrtz_Task *task = (qrtz_Task *)obj;
		qrtz_freeArray(vm, task->stack, sizeof(qrtz_Value), task->stackcap);
		qrtz_freeArray(vm, task->calls, sizeof(qrtz_Value), task->callcap);
		qrtz_free(vm, task, sizeof(qrtz_Pointer));
		return;
	}
}

qrtz_VM *qrtz_create(qrtz_Context *ctx) {
	qrtz_VM *vm = qrtz_calloc(ctx, sizeof(*vm));
	if(vm == NULL) return NULL;
	vm->ctx = *ctx;
	vm->heap = NULL;
	vm->graySet = NULL;
	vm->memUsage = sizeof(qrtz_VM);
	vm->memTarget = 200 * 1024;
	vm->gcPause = 2;

	vm->globals = qrtz_allocMapObject(vm, 16);
	if(vm->globals == NULL) goto fail;
	vm->registry = qrtz_allocMapObject(vm, 16);
	if(vm->registry == NULL) goto fail;
	vm->loaded = qrtz_allocMapObject(vm, 16);
	if(vm->loaded == NULL) goto fail;
	vm->mainTask = qrtz_allocTaskObject(vm, vm->globals);
	if(vm->mainTask == NULL) goto fail;
	vm->curTask = vm->mainTask;
	const char *oom = "out of memory";
	vm->oomStr = qrtz_allocStringObject(vm, oom, qrtz_strlen(oom));
	if(vm->oomStr == NULL) goto fail;

	return vm;
fail:
	qrtz_destroy(vm);
	return NULL;
}

void qrtz_destroy(qrtz_VM *vm) {
	qrtz_Context ctx = vm->ctx;

	qrtz_Object *obj = vm->heap;
	while(obj != NULL) {
		qrtz_Object *cur = obj;
		obj = obj->next;
		qrtz_objfree(vm, cur);
	}

	qrtz_cfree(&ctx, vm, sizeof(qrtz_VM));
}

qrtz_Context *qrtz_contextOf(qrtz_VM *vm) {
	return &vm->ctx;
}

size_t qrtz_objmemsizeof(qrtz_Object *obj);

size_t qrtz_strhash(const char *s, size_t len) {
	// slightly modified version of https://en.wikipedia.org/wiki/Jenkins_hash_function
	size_t hash = 53;
	for(size_t i = 0; i < len; i++) {
		hash += (unsigned char)s[i];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

size_t qrtz_valhash(qrtz_Value val) {
	if(val.tag == QRTZ_VNULL) return 0;
	if(val.tag == QRTZ_VBOOL) return val.boolean ? 1 : 0;
	if(val.tag == QRTZ_VINT) return val.integer;
	if(val.tag == QRTZ_VNUMBER) {
		if(isnan(val.number)) return 0;
		if(isinf(val.number)) return 0;
		return val.number;
	}
	if(val.tag == QRTZ_VCFUNC) return (size_t)val.cfunc;
	if(val.tag != QRTZ_VOBJ) return 0;
	qrtz_Object *o = val.object;
	if(o->tag != QRTZ_OSTR) {
		return ((qrtz_String *)o)->hash;
	}
	return (size_t)o;
}

bool qrtz_hasError(qrtz_VM *vm) {
	return vm->curTask->error.tag != QRTZ_VILLEGAL;
}

qrtz_Exit qrtz_pusherror(qrtz_VM *vm);
qrtz_Exit qrtz_pushoom(qrtz_VM *vm);
qrtz_Exit qrtz_seterror(qrtz_VM *vm, int x);

void qrtz_setoom(qrtz_VM *vm) {
	vm->curTask->error = (qrtz_Value) {
		.tag = QRTZ_VOBJ,
		.object = &vm->oomStr->obj,
	};
}

void qrtz_clearerror(qrtz_VM *vm) {
	vm->curTask->error.tag = QRTZ_VILLEGAL;
}

void qrtz_seterroras(qrtz_VM *vm, qrtz_Exit exit) {
	const char *msg;
	switch(exit) {
	case QRTZ_OK:
		qrtz_clearerror(vm);
		return;
	case QRTZ_ENOMEM:
		qrtz_setoom(vm);
		return;
	case QRTZ_EIO:
		msg = "bad I/O";
		return;
	case QRTZ_ENOSTACK:
		msg = "stack overflow";
		return;
	case QRTZ_ESYNTAX:
		msg = "syntax error";
		return;
	case QRTZ_ERUNTIME:
		msg = "internal error";
		return;
	}

	qrtz_String *s = qrtz_allocStringObject(vm, msg, qrtz_strlen(msg));
	if(s == NULL) {
		qrtz_setoom(vm);
		return;
	}
	vm->curTask->error = (qrtz_Value) {
		.tag = QRTZ_VOBJ,
		.object = &s->obj,
	};
}
