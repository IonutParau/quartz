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
		if((t->hash & 1) == 1) return t->hash;
		// potential stackoverflow here.
		// after constructing tuples, the hashing should be baked
		size_t hash = 0;
		for(size_t i = 0; i < t->len; i++) {
			// yeah I'm just BS-ing here
			hash ^= (hash << 3) | quartzI_hash(t->vals[i]);
		}
		return t->hash = (hash<<1) | 1;
	}
	return (size_t)obj;
}

size_t quartzI_memsizeof(quartz_Value val) {
	// memsizeof is the amount of memory *retained*
	if(val.type != QUARTZ_VOBJ) return 0;

	quartz_Object *obj = val.obj;
	// TODO: memsizeof
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
	if(obj->type == QUARTZ_OLIST) return QUARTZ_TLIST;
	if(obj->type == QUARTZ_OTUPLE) return QUARTZ_TTUPLE;
	if(obj->type == QUARTZ_OSET) return QUARTZ_TSET;
	if(obj->type == QUARTZ_OSTRUCT) return QUARTZ_TSTRUCT;
	if(obj->type == QUARTZ_OFUNCTION) return QUARTZ_TFUNCTION;
	if(obj->type == QUARTZ_OCLOSURE) return QUARTZ_TFUNCTION;
	if(obj->type == QUARTZ_OPOINTER) return QUARTZ_TPOINTER;
	if(obj->type == QUARTZ_OTHREAD) return QUARTZ_TTHREAD;
	if(obj->type == QUARTZ_OUSERDATA) return QUARTZ_TUSERDATA;
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

bool quartzI_isInterpretedFunction(quartz_Value f) {
	if(f.type == QUARTZ_VCFUNC) return false;
	if(f.type != QUARTZ_VOBJ) return false;
	if(f.obj->type == QUARTZ_OFUNCTION) return false;
	if(f.obj->type != QUARTZ_OCLOSURE) return false;
	quartz_Closure *c = (quartz_Closure *)f.obj;
	return quartzI_isInterpretedFunction(c->f);
}

quartz_Closure *quartzI_getClosure(quartz_Value f) {
	if(f.type != QUARTZ_VOBJ) return false;
	if(f.obj->type != QUARTZ_OCLOSURE) return false;
	return (quartz_Closure *)f.obj;
}
