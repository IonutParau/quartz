#ifndef QUARTZ_STATE_H
#define QUARTZ_STATE_H

#include "quartz.h"
#include "value.h"

quartz_Value quartzI_currentErrorHandler(quartz_Thread *Q);
quartz_Errno quartzI_invokeErrorHandler(quartz_Thread *Q, quartz_Errno oerr);
quartz_Errno quartzI_ensureStackSize(quartz_Thread *Q, size_t size);
quartz_Errno quartzI_addCallEntry(quartz_Thread *Q, quartz_CallEntry *entry);
void quartzI_popCallEntry(quartz_Thread *Q);
quartz_CallEntry *quartzI_getCallEntry(quartz_Thread *Q, size_t off);
bool quartzI_isCurrentlyInterpreted(quartz_Thread *Q);
quartz_Errno quartzI_runTopEntry(quartz_Thread *Q);
quartz_Value quartzI_getStackValue(quartz_Thread *Q, int x);
quartz_Pointer *quartzI_getStackValuePointer(quartz_Thread *Q, int x, quartz_Errno *err);
void quartzI_setStackValue(quartz_Thread *Q, int x, quartz_Value v);
quartz_Errno quartzI_pushRawValue(quartz_Thread *Q, quartz_Value v);
quartz_Errno quartzI_getFunctionIndex(quartz_Thread *Q, size_t *idx);
// index into the real stack
size_t quartzI_stackFrameOffset(quartz_Thread *Q);

#endif
