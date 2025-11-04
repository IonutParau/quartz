#ifndef QUARTZ_STATE_H
#define QUARTZ_STATE_H

#include "quartz.h"
#include "value.h"

quartz_Errno quartzI_invokeErrorHandler(quartz_Thread *Q);
quartz_Errno quartzI_ensureStackSize(quartz_Thread *Q, size_t size);
quartz_Value quartzI_getStackValue(quartz_Thread *Q, int x);
void quartzI_setStackValue(quartz_Thread *Q, int x, quartz_Value v);

#endif
