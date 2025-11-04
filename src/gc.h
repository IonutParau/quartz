#ifndef QUARTZ_GC_H
#define QUARTZ_GC_H

#include "quartz.h"
#include "value.h"

quartz_Object *quartzI_allocObject(quartz_Thread *Q, quartz_ObjectType type, size_t size);

quartz_String *quartzI_newString(quartz_Thread *Q, size_t len, const char *mem);
quartz_String *quartzI_newCString(quartz_Thread *Q, const char *s);
quartz_List *quartzI_newList(quartz_Thread *Q, size_t cap);
quartz_Tuple *quartzI_newTuple(quartz_Thread *Q, size_t len);
quartz_Set *quartzI_newSet(quartz_Thread *Q, size_t cap);
quartz_Map *quartzI_newMap(quartz_Thread *Q);
quartz_Struct *quartzI_newStruct(quartz_Thread *Q, quartz_Tuple *fields);
quartz_Pointer *quartzI_newPointer(quartz_Thread *Q);
quartz_Thread *quartzI_newThread(quartz_Thread *Q);

void quartzI_emptyTemporaries(quartz_Thread *Q);
void quartzI_freeObject(quartz_Thread *Q, quartz_Object *obj);

#endif
