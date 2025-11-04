#include "value.h"
#include <math.h>

bool quartzI_validKey(quartz_Value val) {
	if(val.type == QUARTZ_VNULL) return false;
	if(val.type == QUARTZ_VNUM) {
		return !isnan(val.real);
	}
	if(val.type == QUARTZ_VCOMPLEX) {
		return (!isnan(val.complex.real)) && (!isnan(val.complex.imaginary));
	}
	return true;
}

size_t quartzI_hash(quartz_Value val) {
	// NaN is disallowed as key btw dw
	if(val.type != QUARTZ_VOBJ) return (size_t)val.obj; // bitcast is fine
	quartz_Object *obj = val.obj;
	if(obj->type == QUARTZ_OSTR) {
		quartz_String *s = (quartz_String *)obj;
		if(s->hash != 0) return s->hash;
		// taken from https://stackoverflow.com/questions/7666509/hash-function-for-string
		// yes, first result from Google, 2nd comment from SO, not ideal, TODO: use a better hash function
		size_t hash = 0;
		for(size_t i = 0; i < s->len; i++) {
			hash *= 17000069;
			hash += s->buf[i];
		}
		return s->hash = hash;
	}
	if(obj->type == QUARTZ_OTUPLE) {
		quartz_Tuple *t = (quartz_Tuple *)obj;
		if(t->hash != 0) return t->hash;
		size_t hash = 0;
		for(size_t i = 0; i < t->len; i++) {
			// yeah I'm just BS-ing here
			hash ^= (hash << 3) | quartzI_hash(t->vals[i]);
		}
		return t->hash = hash;
	}
	return (size_t)obj;
}

size_t quartzI_memsizeof(quartz_Value val) {
	// memsizeof is the amount of memory *retained*
	if(val.type != QUARTZ_VOBJ) return 0;

	quartz_Object *obj = val.obj;
	return 0;
}

quartz_Type quartzI_trueTypeOf(quartz_Value v) {
	if(v.type == QUARTZ_VNULL) return QUARTZ_TNULL;
	if(v.type == QUARTZ_VINT) return QUARTZ_TINT;
	if(v.type == QUARTZ_VNUM) return QUARTZ_TREAL;
	if(v.type == QUARTZ_VBOOL) return QUARTZ_TBOOL;
	if(v.type == QUARTZ_VCOMPLEX) return QUARTZ_TCOMPLEX;
	if(v.type == QUARTZ_VCFUNC) return QUARTZ_TFUNCTION;

	quartz_Object *obj = v.obj;
	if(obj->type == QUARTZ_OSTR) return QUARTZ_TSTR;
	return QUARTZ_TNULL; // idfk
}

const char *quartz_typenames[QUARTZ_TCOUNT] = {
	[QUARTZ_TNULL] = "null",
	[QUARTZ_TBOOL] = "bool",
	[QUARTZ_TINT] = "int",
	[QUARTZ_TREAL] = "real",
	[QUARTZ_TCOMPLEX] = "complex",
	[QUARTZ_TSTR] = "string",
	[QUARTZ_TLIST] = "list",
	[QUARTZ_TTUPLE] = "tuple",
	[QUARTZ_TSET] = "set",
	[QUARTZ_TMAP] = "map",
	[QUARTZ_TSTRUCT] = "struct",
	[QUARTZ_TFUNCTION] = "function",
	[QUARTZ_TPOINTER] = "pointer",
	[QUARTZ_TTHREAD] = "thread",
	[QUARTZ_TUSERDATA] = "userdata",
};

bool quartzI_equals(quartz_Value a, quartz_Value b);
