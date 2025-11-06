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

quartz_CmpFlags quartzI_compare(quartz_Value a, quartz_Value b) {
	quartz_CmpFlags flags = 0;
	if(a.type == QUARTZ_VINT) {
		if(b.type == QUARTZ_VINT) {
			if(a.integer == b.integer) flags |= QUARTZ_CMP_EQUAL;
			if(a.integer < b.integer) flags |= QUARTZ_CMP_LESS;
			if(a.integer > b.integer) flags |= QUARTZ_CMP_GREATER;
		}
		if(b.type == QUARTZ_VNUM) {
			if(a.integer == b.real) flags |= QUARTZ_CMP_EQUAL;
			if(a.integer < b.real) flags |= QUARTZ_CMP_LESS;
			if(a.integer > b.real) flags |= QUARTZ_CMP_GREATER;
		}
		if(b.type == QUARTZ_VCOMPLEX) {
			quartz_Complex c = b.complex;
			if(c.imaginary == 0) {
				if(a.integer == c.real) flags |= QUARTZ_CMP_EQUAL;
				if(a.integer < c.real) flags |= QUARTZ_CMP_LESS;
				if(a.integer > c.real) flags |= QUARTZ_CMP_GREATER;
			}
		}
	}
	if(a.type == QUARTZ_VNUM) {
		if(b.type == QUARTZ_VINT) {
			if(a.real == b.integer) flags |= QUARTZ_CMP_EQUAL;
			if(a.real < b.integer) flags |= QUARTZ_CMP_LESS;
			if(a.real > b.integer) flags |= QUARTZ_CMP_GREATER;
		}
		if(b.type == QUARTZ_VNUM) {
			if(a.real == b.real) flags |= QUARTZ_CMP_EQUAL;
			if(a.real < b.real) flags |= QUARTZ_CMP_LESS;
			if(a.real > b.real) flags |= QUARTZ_CMP_GREATER;
		}
		if(b.type == QUARTZ_VCOMPLEX) {
			quartz_Complex c = b.complex;
			if(c.imaginary == 0) {
				if(a.real == c.real) flags |= QUARTZ_CMP_EQUAL;
				if(a.real < c.real) flags |= QUARTZ_CMP_LESS;
				if(a.real > c.real) flags |= QUARTZ_CMP_GREATER;
			}
		}
	}
	if(a.type == QUARTZ_VCOMPLEX) {
		quartz_Complex c = a.complex;
		if(b.type == QUARTZ_VINT) {
			if(c.imaginary == 0) {
				if(c.real == b.integer) flags |= QUARTZ_CMP_EQUAL;
				if(c.real < b.integer) flags |= QUARTZ_CMP_LESS;
				if(c.real > b.integer) flags |= QUARTZ_CMP_GREATER;
			}
		}
		if(b.type == QUARTZ_VNUM) {
			if(c.imaginary == 0) {
				if(c.real == b.real) flags |= QUARTZ_CMP_EQUAL;
				if(c.real < b.real) flags |= QUARTZ_CMP_LESS;
				if(c.real > b.real) flags |= QUARTZ_CMP_GREATER;
			}
		}
		if(b.type == QUARTZ_VCOMPLEX) {
			quartz_Complex c2 = b.complex;
			if(c.imaginary == c2.imaginary) {
				if(c.real == c2.real) flags |= QUARTZ_CMP_EQUAL;
				if(c.real < c2.real) flags |= QUARTZ_CMP_LESS;
				if(c.real > c2.real) flags |= QUARTZ_CMP_GREATER;
			}
		}
	}
	if(a.type == QUARTZ_VOBJ && b.type == QUARTZ_VOBJ) {
		if(a.obj == b.obj) flags |= QUARTZ_CMP_EQUAL;
	}
	return flags;
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

quartz_Value quartzI_mapGet(quartz_Map *m, quartz_Value key) {
	if(!quartzI_validKey(key)) return (quartz_Value) {.type = QUARTZ_VNULL};
	size_t hash = quartzI_hash(key);
	size_t i = hash;
	size_t left = m->capacity;
	while(left > 0) {
		i %= m->capacity;
		if(m->pairs[i].key.type == QUARTZ_VNULL) break;
		if(quartzI_compare(m->pairs[i].key, key) & QUARTZ_CMP_EQUAL) {
			return m->pairs[i].val;
		}
		i++;
		left--;
	}
	return (quartz_Value) {.type = QUARTZ_VNULL};
}

quartz_Errno quartzI_mapSet(quartz_Thread *Q, quartz_Map *m, quartz_Value key, quartz_Value v) {
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "not implemented yet");
}

quartz_Errno quartzI_getIndex(quartz_Thread *Q, quartz_Value container, quartz_Value key, quartz_Value *val) {
	val->type = QUARTZ_VNULL;
	if(container.type != QUARTZ_VOBJ) goto typeError;
	quartz_Object *o = container.obj;
	if(o->type == QUARTZ_OLIST) {
		quartz_List *l = (quartz_List *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= l->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		*val = l->vals[key.integer];
		goto ok;
	}
	if(o->type == QUARTZ_OTUPLE) {
		quartz_Tuple *t = (quartz_Tuple *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= t->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		*val = t->vals[key.integer];
		goto ok;
	}
	if(o->type == QUARTZ_OSET) {
		quartz_Set *l = (quartz_Set *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= l->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		*val = l->vals[key.integer];
		goto ok;
	}
	if(o->type == QUARTZ_OMAP) {
		quartz_Map *m = (quartz_Map *)o;
		*val = quartzI_mapGet(m, key);
		goto ok;
	}
typeError:
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "container expected, got %s", quartz_typenames[quartzI_trueTypeOf(container)]);
badKeyType:
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "illegal key type: %s", quartz_typenames[quartzI_trueTypeOf(key)]);
ok:
	return QUARTZ_OK;
}

quartz_Errno quartzI_setIndex(quartz_Thread *Q, quartz_Value container, quartz_Value key, quartz_Value val) {
	if(container.type != QUARTZ_VOBJ) goto typeError;
	quartz_Object *o = container.obj;
	if(o->type == QUARTZ_OLIST) {
		quartz_List *l = (quartz_List *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= l->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		l->vals[key.integer] = val;
		goto ok;
	}
	if(o->type == QUARTZ_OTUPLE) {
		quartz_Tuple *t = (quartz_Tuple *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= t->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		t->vals[key.integer] = val;
		goto ok;
	}
	if(o->type == QUARTZ_OSET) {
		quartz_Set *l = (quartz_Set *)o;
		if(key.type != QUARTZ_VINT) goto badKeyType;
		if(key.integer < 0 || key.integer >= l->len) {
			return quartz_errorf(Q, QUARTZ_ERUNTIME, "index out of bounds");
		}
		l->vals[key.integer] = val;
		goto ok;
	}
	if(o->type == QUARTZ_OMAP) {
		quartz_Map *m = (quartz_Map *)o;
		return quartzI_mapSet(Q, m, key, val);
	}
typeError:
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "container expected, got %s", quartz_typenames[quartzI_trueTypeOf(container)]);
badKeyType:
	return quartz_errorf(Q, QUARTZ_ERUNTIME, "illegal key type: %s", quartz_typenames[quartzI_trueTypeOf(key)]);
ok:
	return QUARTZ_OK;
}
