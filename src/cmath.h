#ifndef QUARTZ_CMATH_H
#define QUARTZ_CMATH_H

// Quartz complex number math

#include "quartz.h" // defines the quartz_Complex, quartz_Int and quartz_Real types

// represents c as r * e ** (theta * i), and outputs r and theta.
void quartzCM_splitEuler(quartz_Complex *c, quartz_Real *r, quartz_Real *theta);

// computes r * e ** (theta * i)
void quartzCM_fromEuler(quartz_Complex *c, quartz_Real r, quartz_Real theta);

// fast maths
void quartzCM_complexAddReal(quartz_Complex *c, quartz_Real x);
void quartzCM_complexSubReal(quartz_Complex *c, quartz_Real x);
void quartzCM_complexMltReal(quartz_Complex *c, quartz_Real x);
void quartzCM_complexDivReal(quartz_Complex *c, quartz_Real x);
void quartzCM_complexExpInt(quartz_Complex *c, quartz_Int x);
void quartzCM_complexExpReal(quartz_Complex *c, quartz_Real x);
void quartzCM_complexExpI(quartz_Complex *c);
// computes 1/c, which can be used as a fast path for x/c
void quartzCM_complexInv(quartz_Complex *c);

// a = a + b
void quartzCM_complexAdd(quartz_Complex *a, quartz_Complex *b);
// a = a - b
void quartzCM_complexSub(quartz_Complex *a, quartz_Complex *b);
// a = a * b
void quartzCM_complexMlt(quartz_Complex *a, quartz_Complex *b);
// a = a / b
void quartzCM_complexDiv(quartz_Complex *a, quartz_Complex *b);
// a = a ** b
void quartzCM_complexExp(quartz_Complex *a, quartz_Complex *b);

#endif
