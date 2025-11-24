#include "value.h"
#include "quartz.h"
#include "gc.h"
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
	if(obj->type == QUARTZ_OSTR) {
		quartz_String *s = (quartz_String *)obj;
		return sizeof(*s) + s->len + 1;
	}
	if(obj->type == QUARTZ_OLIST) {
		quartz_List *l = (quartz_List *)obj;
		return sizeof(*l) + l->cap * sizeof(quartz_Value);
	}
	if(obj->type == QUARTZ_OTUPLE) {
		quartz_Tuple *tup = (quartz_Tuple *)obj;
		return sizeof(*tup) + tup->len * sizeof(quartz_Value);
	}
	if(obj->type == QUARTZ_OSET) {
		quartz_Set *s = (quartz_Set *)obj;
		return sizeof(*s) + s->cap * sizeof(quartz_Value);
	}
	if(obj->type == QUARTZ_OMAP) {
		quartz_Map *m = (quartz_Map *)obj;
		return sizeof(*m) + m->capacity * sizeof(quartz_MapPair);
	}
	if(obj->type == QUARTZ_OSTRUCT) {
		quartz_Struct *s = (quartz_Struct *)obj;
		size_t fieldLen = s->fields->len;
		return sizeof(*s) + fieldLen * sizeof(quartz_Value);
	}
	if(obj->type == QUARTZ_OFUNCTION) {
		quartz_Function *f = (quartz_Function *)obj;
		size_t m = sizeof(*f);
		m += sizeof(f->code[0]) * f->codesize;
		m += sizeof(f->consts[0]) * f->constCount;
		m += sizeof(f->upvaldefs[0]) * f->upvalCount;
		return m;
	}
	if(obj->type == QUARTZ_OCLOSURE) {
		quartz_Closure *c = (quartz_Closure *)obj;
		return sizeof(*c) + c->len * sizeof(quartz_Value);
	}
	if(obj->type == QUARTZ_OPOINTER) {
		return sizeof(quartz_Pointer);
	}
	if(obj->type == QUARTZ_OTHREAD) {
		quartz_Thread *Q = (quartz_Thread *)obj;
		return sizeof(*Q) + sizeof(quartz_StackEntry) * Q->stackCap + sizeof(quartz_CallEntry) * Q->callCap;
	}
	if(obj->type == QUARTZ_OUSERDATA) {
		quartz_Userdata *u = (quartz_Userdata *)obj;
		return sizeof(*u) + u->userdataSize + sizeof(quartz_Value) * u->associatedLen;
	}
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
	if(obj->type == QUARTZ_OMAP) return QUARTZ_TMAP;
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
	if(a.type == QUARTZ_VNULL) {
		if(b.type == QUARTZ_VNULL) flags |= QUARTZ_CMP_EQUAL;
	}
	if(a.type == QUARTZ_VBOOL) {
		if(b.type == QUARTZ_VBOOL && a.b == b.b) flags |= QUARTZ_CMP_EQUAL;
	}
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
		quartz_Object *oA = a.obj;
		quartz_Object *oB = b.obj;
		if(oA == oB) flags |= QUARTZ_CMP_EQUAL;
		else if(oA->type == QUARTZ_OSTR && oB->type == QUARTZ_OSTR) {
			quartz_String *sA = (quartz_String *)oA;
			quartz_String *sB = (quartz_String *)oB;
			
			int cmp = 0;
			size_t len = sA->len > sB->len ? sA->len : sB->len;
			for(size_t i = 0; i < len; i++) {
				unsigned char a = sA->buf[i];
				unsigned char b = sB->buf[i];
				if((cmp = b - a)) {
					break;
				}
			}
		
			if(cmp == 0) flags |= QUARTZ_CMP_EQUAL;
			if(cmp < 0) flags |= QUARTZ_CMP_LESS;
			if(cmp > 0) flags |= QUARTZ_CMP_GREATER;
		}
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
	if(f.obj->type == QUARTZ_OFUNCTION) return true;
	if(f.obj->type != QUARTZ_OCLOSURE) return false;
	quartz_Closure *c = (quartz_Closure *)f.obj;
	return quartzI_isInterpretedFunction(c->f);
}

quartz_Closure *quartzI_getClosure(quartz_Value f) {
	if(f.type != QUARTZ_VOBJ) return false;
	if(f.obj->type != QUARTZ_OCLOSURE) return false;
	return (quartz_Closure *)f.obj;
}

quartz_Function *quartzI_getFunction(quartz_Value f) {
	if(f.type != QUARTZ_VOBJ) return NULL;
	quartz_Object *o = f.obj;
	if(o->type == QUARTZ_OCLOSURE) {
		quartz_Closure *c = (quartz_Closure *)o;
		return quartzI_getFunction(c->f);
	}
	if(o->type != QUARTZ_OFUNCTION) return NULL;
	return (quartz_Function *)o;
}

quartz_CFunction *quartzI_getCFunction(quartz_Value f) {
	if(f.type == QUARTZ_VOBJ) {
		quartz_Object *o = f.obj;
		if(o->type != QUARTZ_OCLOSURE) return NULL;
		quartz_Closure *c = (quartz_Closure *)o;
		return quartzI_getCFunction(c->f);
	}
	if(f.type != QUARTZ_VCFUNC) return NULL;
	return f.func;
}

quartz_Value quartzI_mapGet(quartz_Map *m, quartz_Value key) {
	if(!quartzI_validKey(key)) return (quartz_Value) {.type = QUARTZ_VNULL};
	size_t hash = quartzI_hash(key);
	size_t i = hash;
	size_t left = m->capacity;
	while(left > 0) {
		i %= m->capacity;
		// unallocated
		if(m->pairs[i].key.type == QUARTZ_VNULL) break;
		if(quartzI_compare(m->pairs[i].key, key) & QUARTZ_CMP_EQUAL) {
			return m->pairs[i].val;
		}
		i++;
		left--;
	}
	return (quartz_Value) {.type = QUARTZ_VNULL};
}

// returns whether it allocated new space
static bool quartzI_mapPutInArray(quartz_MapPair *pairs, size_t cap, quartz_Value key, quartz_Value val) {
	// assumes there is always free space, basically, that allocated space is less than capacity
	size_t i = quartzI_hash(key);
	while(true) {
		i %= cap;
		if(pairs[i].key.type == QUARTZ_VNULL) {
			// unallocated space!!!
			pairs[i].key = key;
			pairs[i].val = val;
			return true;
		}
		if(pairs[i].val.type == QUARTZ_VNULL) {
			pairs[i].key = key;
			pairs[i].val = val;
			return false;
		}
		if(quartzI_compare(pairs[i].key, key) & QUARTZ_CMP_EQUAL) {
			pairs[i].val = val;
			return false;
		}
		i++;
	}
	return false;
}

bool quartzI_truthyValue(quartz_Value v) {
	if(v.type == QUARTZ_VNULL) return false;
	if(v.type == QUARTZ_VBOOL) return v.b;
	return true;
}

bool quartzI_isLegalPair(quartz_MapPair pair) {
	return pair.key.type != QUARTZ_VNULL && pair.val.type != QUARTZ_VNULL;
}

quartz_Errno quartzI_mapSet(quartz_Thread *Q, quartz_Map *m, quartz_Value key, quartz_Value v) {
	size_t idealMaxAllocated = m->capacity * 80 / 100;
	if(m->filledAmount >= idealMaxAllocated) {
		// welp, time to re-size.
		size_t newCap = m->capacity * 2;
		quartz_MapPair *newBuf = quartz_alloc(Q, newCap * sizeof(*newBuf));
		if(newBuf == NULL) return quartz_oom(Q);
		for(size_t i = 0; i < newCap; i++) {
			newBuf[i].key.type = QUARTZ_VNULL;
			newBuf[i].val.type = QUARTZ_VNULL;
		}
		size_t newSpaceAllocated = 0;
		for(size_t i = 0; i < m->capacity; i++) {
			if(quartzI_isLegalPair(m->pairs[i])) {
				if(quartzI_mapPutInArray(newBuf, newCap, m->pairs[i].key, m->pairs[i].val)) newSpaceAllocated++;
			}
		}
		quartz_free(Q, m->pairs, sizeof(quartz_MapPair) * m->capacity);
		m->pairs = newBuf;
		m->capacity = newCap;
		m->filledAmount = newSpaceAllocated;
	}
	if(quartzI_mapPutInArray(m->pairs, m->capacity, key, v)) {
		m->filledAmount++;
	}
	return QUARTZ_OK;
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
		return quartz_errorf(Q, QUARTZ_ERUNTIME, "tuples are immutable");
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

quartz_String *quartzI_valueToString(quartz_Thread *Q, quartz_Value v) {
	quartz_Type t = quartzI_trueTypeOf(v);
	if(t == QUARTZ_TSTR) {
		return (quartz_String *)v.obj;
	}
	if(t == QUARTZ_TNULL) {
		return quartzI_newCString(Q, "null");
	}
	if(t == QUARTZ_TBOOL) {
		return quartzI_newCString(Q, v.b ? "true" : "false");
	}
	if(t == QUARTZ_TINT) {
		return quartzI_newFString(Q, "%d", v.integer);
	}
	if(t == QUARTZ_TREAL) {
		return quartzI_newFString(Q, "%f", v.real);
	}
	if(t == QUARTZ_TCOMPLEX) {
		return quartzI_newFString(Q, "%C", v.complex);
	}
	return quartzI_newFString(Q, "<%s at %p>", quartz_typenames[t], v.obj);
}

quartz_String *quartzI_valueQuoted(quartz_Thread *Q, quartz_Value v) {
	quartz_Type t = quartzI_trueTypeOf(v);
	if(t == QUARTZ_TSTR) {
		quartz_String *s = (quartz_String *)v.obj;
		return quartzI_newFString(Q, "%Q", s->len, s->buf);
	}
	if(t == QUARTZ_TNULL) {
		return quartzI_newCString(Q, "null");
	}
	if(t == QUARTZ_TBOOL) {
		return quartzI_newCString(Q, v.b ? "true" : "false");
	}
	if(t == QUARTZ_TINT) {
		return quartzI_newFString(Q, "%d", v.integer);
	}
	if(t == QUARTZ_TREAL) {
		return quartzI_newFString(Q, "%f", v.real);
	}
	if(t == QUARTZ_TCOMPLEX) {
		return quartzI_newFString(Q, "%C", v.complex);
	}
	return quartzI_newFString(Q, "<unquotable>", quartz_typenames[t], v.obj);
}
