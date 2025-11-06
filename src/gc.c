#include "quartz.h"
#include "value.h"
#include "gc.h"
#include "utils.h"

static bool quartzI_addTmpObj(quartz_Thread *Q, quartz_Object *o) {
	if(Q->gState->tmpArrSize == QUARTZ_MAX_TMP) return true;
	Q->gState->tmpArr[Q->gState->tmpArrSize++] = o;
	return false;
}

size_t quartz_gcCount(quartz_Thread *Q) {
	return Q->gState->gcCount;
}

size_t quartz_gcTarget(quartz_Thread *Q) {
	return Q->gState->gcTarget;
}

size_t quartz_gcPeak(quartz_Thread *Q) {
	return Q->gState->gcPeak;
}

double quartz_gcRatio(quartz_Thread *Q, double ratio) {
	return Q->gState->gcRatio;
}

static void quartz_grayValue(quartz_Thread *Q, quartz_Value value);

static void quartz_markObject(quartz_Thread *Q, quartz_Object *obj) {
	obj->flags |= QUARTZ_FLAG_MARKED;
	if(obj->flags & QUARTZ_FLAG_MARKED) return;

	// TODO: gray everything
}

static void quartz_grayValue(quartz_Thread *Q, quartz_Value value) {
	if(value.type != QUARTZ_VOBJ) return;
	quartz_Object *obj = value.obj;
	if(obj->flags & QUARTZ_FLAG_GRAY) return;
	obj->flags |= QUARTZ_FLAG_GRAY;
	obj->nextGray = Q->gState->graySet;
	Q->gState->graySet = obj;
}

void quartz_gc(quartz_Thread *Q) {
	if(Q->gState->gcBlocked) return; // not allowed
	// TODO: garbage collection
}

quartz_Object *quartzI_allocObject(quartz_Thread *Q, quartz_ObjectType type, size_t size) {
	quartz_Object *obj = quartz_alloc(Q, size);
	if(obj == NULL) return NULL;
	obj->type = type;
	obj->nextGray = NULL;
	obj->flags = 0;
	if(quartzI_addTmpObj(Q, obj)) {
		quartz_free(Q, obj, size);
		return NULL;
	}
	obj->nextObject = Q->gState->objList;
	Q->gState->objList = obj;
	return obj;
}

quartz_String *quartzI_newString(quartz_Thread *Q, size_t len, const char *mem) {
	quartz_String *s = (quartz_String *)quartzI_allocObject(Q, QUARTZ_OSTR, sizeof(quartz_String) + len + 1);
	if(s == NULL) return NULL;
	s->len = len;
	s->hash = 0;
	if(mem != NULL) quartzI_memcpy(s->buf, mem, len);
	s->buf[len] = '\0';
	return s;
}

quartz_String *quartzI_newCString(quartz_Thread *Q, const char *s) {
	size_t len = quartzI_strlen(s);
	return quartzI_newString(Q, len, s);
}

quartz_List *quartzI_newList(quartz_Thread *Q, size_t cap) {
	if(cap == 0) cap = 8;
	quartz_Value *buf = quartz_alloc(Q, sizeof(quartz_Value) * cap);
	if(buf == NULL) return NULL;
	quartz_List *l = (quartz_List *)quartzI_allocObject(Q, QUARTZ_OLIST, sizeof(*l));
	if(l == NULL) {
		quartz_free(Q, buf, sizeof(quartz_Value) * cap);
		return NULL;
	}
	l->len = 0;
	l->cap = cap;
	l->vals = buf;
	return l;
}

quartz_Tuple *quartzI_newTuple(quartz_Thread *Q, size_t len) {
	quartz_Tuple *t = (quartz_Tuple *)quartzI_allocObject(Q, QUARTZ_OTUPLE, sizeof(*t) + sizeof(quartz_Value) * len);
	if(t == NULL) return NULL;
	t->len = len;
	t->hash = 0;
	for(size_t i = 0; i < len; i++) t->vals[i].type = QUARTZ_VNULL;
	return t;
}

quartz_Set *quartzI_newSet(quartz_Thread *Q, size_t cap) {
	if(cap == 0) cap = 1;
	quartz_Value *buf = quartz_alloc(Q, sizeof(quartz_Value) * cap);
	if(buf == NULL) return NULL;
	quartz_Set *s = (quartz_Set *)quartzI_allocObject(Q, QUARTZ_OSET, sizeof(*s));
	if(s == NULL) {
		quartz_free(Q, buf, sizeof(quartz_Value) * cap);
		return NULL;
	}
	s->len = 0;
	s->cap = cap;
	s->vals = buf;
	return s;
}

quartz_Map *quartzI_newMap(quartz_Thread *Q, size_t cap) {
	if(cap == 0) cap = 4;
	size_t pairSize = sizeof(quartz_MapPair) * cap;
	quartz_MapPair *pairs = quartz_alloc(Q, pairSize);
	if(pairs == NULL) return NULL;
	for(size_t i = 0; i < cap; i++) {
		// mark as unallocated
		pairs[i].key.type = QUARTZ_VNULL;
	}

	quartz_Map *m = (quartz_Map *)quartzI_allocObject(Q, QUARTZ_OMAP, sizeof(quartz_Map));
	if(m == NULL) {
		quartz_free(Q, pairs, pairSize);
		return NULL;
	}
	m->pairs = pairs;
	m->capacity = cap;
	m->filledAmount = 0;
	return m;
}

quartz_Struct *quartzI_newStruct(quartz_Thread *Q, quartz_Tuple *fields) {
	quartz_Struct *s = (quartz_Struct *)quartzI_allocObject(Q, QUARTZ_OSTRUCT, sizeof(quartz_Struct) + sizeof(quartz_Value) * fields->len);
	if(s == NULL) return NULL;
	s->fields = fields;
	for(size_t i = 0; i < s->fields->len; i++) {
		s->pairs[i].type = QUARTZ_VNULL;
	}
	return s;
}

quartz_Pointer *quartzI_newPointer(quartz_Thread *Q) {
	quartz_Pointer *p = (quartz_Pointer *)quartzI_allocObject(Q, QUARTZ_OPOINTER, sizeof(quartz_Pointer));
	if(p == NULL) return NULL;
	p->val.type = QUARTZ_VNULL;
	return p;
}

quartz_Thread *quartzI_newThread(quartz_Thread *Q) {
	size_t defaultStackSize = 32;
	size_t stackBufSize = defaultStackSize * sizeof(quartz_StackEntry);
	quartz_StackEntry *stackBuf = quartz_alloc(Q, stackBufSize);
	if(stackBuf == NULL) return NULL;
	quartz_Thread *t = (quartz_Thread *)quartzI_allocObject(Q, QUARTZ_OTHREAD, sizeof(quartz_Thread));
	if(t == NULL) {
		quartz_free(Q, stackBuf, stackBufSize);
		return NULL;
	}
	t->gState = Q->gState;
	t->resuming = NULL;
	t->resumedBy = NULL;
	t->stackLen = 0;
	t->stackCap = defaultStackSize;
	t->stack = stackBuf;
	return t;
}

void quartzI_emptyTemporaries(quartz_Thread *Q) {
	Q->gState->tmpArrSize = 0;
}

void quartzI_freeObject(quartz_Thread *Q, quartz_Object *obj) {
	// consider using computed gotos if supported
	if(obj->type == QUARTZ_OSTR) {
		quartz_String *s = (quartz_String *)obj;
		quartz_free(Q, s, sizeof(quartz_String) + s->len + 1);
	} else if(obj->type == QUARTZ_OLIST) {
		quartz_List *l = (quartz_List *)obj;
		quartz_free(Q, l->vals, sizeof(quartz_Value) * l->cap);
		quartz_free(Q, l, sizeof(*l));
	} else if(obj->type == QUARTZ_OTUPLE) {
		quartz_Tuple *t = (quartz_Tuple *)obj;
		quartz_free(Q, t, sizeof(*t) + sizeof(quartz_Value) * t->len);
	} else if(obj->type == QUARTZ_OSET) {
		quartz_Set *s = (quartz_Set *)obj;
		quartz_free(Q, s->vals, sizeof(quartz_Value) * s->cap);
		quartz_free(Q, s, sizeof(*s));
	} else if(obj->type == QUARTZ_OMAP) {
		quartz_Map *m = (quartz_Map *)obj;
		size_t pairSize = sizeof(quartz_MapPair) * m->capacity;
		quartz_free(Q, m->pairs, pairSize);
		quartz_free(Q, m, sizeof(*m));
	}
}
