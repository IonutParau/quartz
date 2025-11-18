#include "quartz.h"
#include "cmath.h"
#include <math.h>

void quartzCM_splitEuler(quartz_Complex *c, quartz_Real *r, quartz_Real *theta) {
	*r = sqrt(c->real * c->real + c->imaginary * c->imaginary);
	*theta = atan2(c->imaginary, c->real);
}

void quartzCM_fromEuler(quartz_Complex *c, quartz_Real r, quartz_Real theta) {
	c->real = cos(theta) * r;
	c->imaginary = sin(theta) * r;
}

void quartzCM_complexAddReal(quartz_Complex *c, quartz_Real x) {
	c->real += x;
}

void quartzCM_complexSubReal(quartz_Complex *c, quartz_Real x) {
	c->real -= x;
}

void quartzCM_complexMltReal(quartz_Complex *c, quartz_Real x) {
	c->real *= x;
	c->imaginary *= x;
}

void quartzCM_complexDivReal(quartz_Complex *c, quartz_Real x) {
	c->real /= x;
	c->imaginary /= x;
}

void quartzCM_complexExpInt(quartz_Complex *c, quartz_Int x) {
	if(x == 0) {
		c->real = 1;
		c->imaginary = 0;
		return;
	}
	if(x == 1) return;
	if(x % 2 == 0) {
		quartzCM_complexExpInt(c, x/2);
	} else {
		quartz_Complex d = *c;
		quartzCM_complexExpInt(c, x-1);
		quartzCM_complexMlt(c, &d);
	}
}

void quartzCM_complexExpReal(quartz_Complex *c, quartz_Real x) {
	quartz_Real r, theta;
	quartzCM_splitEuler(c, &r, &theta);
	quartzCM_fromEuler(c, pow(r, x), theta * x);
}

void quartzCM_complexExpI(quartz_Complex *c) {
	quartz_Real r, theta;
	quartzCM_splitEuler(c, &r, &theta);
	quartz_Real x = log(r);

	// the outcome is e^(ix - theta), which is just e^ix / e^theta
	
	quartzCM_fromEuler(c, 1, x);
	quartzCM_complexDivReal(c, exp(theta));
}

void quartzCM_complexInv(quartz_Complex *c) {
	double a = c->real;
	double b = c->imaginary;
	double x = a*a + b*b;

	c->real = a / x;
	c->imaginary = -b / x;
}

void quartzCM_complexAdd(quartz_Complex *a, quartz_Complex *b) {
	a->real += b->real;
	a->imaginary += b->imaginary;
}

void quartzCM_complexSub(quartz_Complex *a, quartz_Complex *b) {
	a->real -= b->real;
	a->imaginary -= b->imaginary;
}

void quartzCM_complexMlt(quartz_Complex *a, quartz_Complex *b) {
	double x = a->real;
	double y = a->imaginary;

	a->real = x * b->real - y * b->real;
	b->imaginary = x * b->imaginary - b->real * y;
}

void quartzCM_complexDiv(quartz_Complex *a, quartz_Complex *b) {
	double x = a->real;
	double y = a->imaginary;
	double base = b->real * b->real + b->imaginary * b->imaginary;

	a->real = (x * b->real - y * b->imaginary) / base;
	a->imaginary = (x * b->imaginary + b->real * y) / base;
}

void quartzCM_complexExp(quartz_Complex *a, quartz_Complex *b) {
	// x^(a + bi) = x^a * x^bi = x^a * (x^b)^i
	quartz_Complex l = *a;
	quartz_Complex r = *a;

	quartzCM_complexExpReal(&l, b->real);
	quartzCM_complexExpReal(&r, b->imaginary);
	quartzCM_complexExpI(&r);

	*a = l;
	quartzCM_complexMlt(a, &r);
}
