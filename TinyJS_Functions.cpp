/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 *
 * Copyright (C) 2009 Pur3 Ltd
 *

 * 42TinyJS
 *
 * A fork of TinyJS with the goal to makes a more JavaScript/ECMA compliant engine
 *
 * Authored / Changed By Armin Diedering <armin@diedering.de>
 *
 * Copyright (C) 2010-2014 ardisoft
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <math.h>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <set>
#include <time.h>
#include <cstdint>
#include <cmath>
#include "TinyJS.h"

using namespace std;
// ----------------------------------------------- Actual Functions

static void scTrace(const CFunctionsScopePtr &c, void * userdata) {
	CTinyJS *js = (CTinyJS*)userdata;
	if(c->getArgumentsLength())
		c->getArgument(0)->trace();
	else
		js->getRoot()->trace("root");
}

static void scObjectDump(const CFunctionsScopePtr &c, void *) {
	c->getArgument("this")->trace("> ");
}

static void scObjectClone(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr obj = c->getArgument("this");
	c->setReturnVar(obj->clone());
}
/*
static void scIntegerValueOf(const CFunctionsScopePtr &c, void *) {
	string str = c->getArgument("str")->toString();

	int val = 0;
	if (str.length()==1)
		val = str.operator[](0);
	c->setReturnVar(c->newScriptVar(val));
}
*/
static string jsonQuote(const string &s) {
	string out;
	out.push_back('"');
	for (size_t i = 0; i < s.size(); ++i) {
		unsigned char ch = (unsigned char)s[i];
		switch (ch) {
		case '"': out.append("\\\""); break;
		case '\\': out.append("\\\\"); break;
		case '\b': out.append("\\b"); break;
		case '\f': out.append("\\f"); break;
		case '\n': out.append("\\n"); break;
		case '\r': out.append("\\r"); break;
		case '\t': out.append("\\t"); break;
		default:
			if (ch < 0x20) {
				static const char *hex = "0123456789abcdef";
				out.append("\\u00");
				out.push_back(hex[ch >> 4]);
				out.push_back(hex[ch & 0x0F]);
			} else
				out.push_back((char)ch);
			break;
		}
	}
	out.push_back('"');
	return out;
}

static bool jsonWrite(const CFunctionsScopePtr &c, const CScriptVarPtr &v, string &out, set<CScriptVar*> &seen, bool asArrayEl);

static void scJSONStringify(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr obj = c->getArgument("obj");
	set<CScriptVar*> seen;
	string out;
	if (!jsonWrite(c, obj, out, seen, false))
		c->setReturnVar(c->constScriptVar(Undefined));
	else
		c->setReturnVar(c->newScriptVar(out));
}

static bool jsonWrite(const CFunctionsScopePtr &c, const CScriptVarPtr &v, string &out, set<CScriptVar*> &seen, bool asArrayEl) {
	if (!v || v->isUndefined() || v->isFunction() || v->isAccessor()) {
		if (asArrayEl) { out.append("null"); return true; }
		return false;
	}
	if (v->isObject()) {
		CScriptVarLinkWorkPtr toJSON = v->findChildWithPrototypeChain("toJSON");
		if (toJSON) {
			CScriptVarPtr fn = toJSON.getter();
			if (fn && fn->isFunction() && seen.find(v.getVar()) == seen.end()) {
				vector<CScriptVarPtr> args;
				CScriptVarPtr replaced = c->getContext()->callFunction(fn, args, v);
				if (replaced.getVar() != v.getVar())
					return jsonWrite(c, replaced, out, seen, asArrayEl);
			}
		}
	}
	if (v->isNull()) { out.append("null"); return true; }
	if (v->isBool()) { out.append(v->toBoolean() ? "true" : "false"); return true; }
	if (v->isNumber()) {
		CNumber n = v->toNumber();
		if (!n.isFinite()) out.append("null");
		else out.append(n.toString());
		return true;
	}
	if (v->isString()) { out.append(jsonQuote(v->toString())); return true; }
	if (v->isObject()) {
		CScriptVar *raw = v.getVar();
		if (seen.find(raw) != seen.end())
			c->throwError(TypeError, "cyclic object value");
		seen.insert(raw);
		if (v->isArray()) {
			uint32_t len = v->getArrayLength();
			out.push_back('[');
			for (uint32_t i = 0; i < len; ++i) {
				if (i) out.push_back(',');
				if (!jsonWrite(c, v->getArrayIndex(i), out, seen, true))
					out.append("null");
			}
			out.push_back(']');
		} else {
			STRING_SET_t names;
			v->keys(names, true);
			out.push_back('{');
			const char *comma = "";
			for (STRING_SET_t::iterator it = names.begin(); it != names.end(); ++it) {
				if (*it == TINYJS___PROTO___VAR) continue;
				CScriptVarPtr val = v->findChildWithStringChars(*it).getter();
				if (!val) continue;
				string piece;
				if (!jsonWrite(c, val, piece, seen, false))
					continue;
				out.append(comma); comma = ",";
				out.append(jsonQuote(*it));
				out.push_back(':');
				out.append(piece);
			}
			out.push_back('}');
		}
		seen.erase(raw);
		return true;
	}
	if (asArrayEl) { out.append("null"); return true; }
	return false;
}

static void scArrayContains(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument("obj");
	CScriptVarPtr arr = c->getArgument("this");

	int l = arr->getArrayLength();
	CScriptVarPtr equal = c->constScriptVar(Undefined);
	for (int i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if(equal->toBoolean()) {
			c->setReturnVar(c->constScriptVar(true));
			return;
		}
	}
	c->setReturnVar(c->constScriptVar(false));
}

static void scArrayRemove(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr obj = c->getArgument("obj");
	CScriptVarPtr arr = c->getArgument("this");
	int i;
	vector<int> removedIndices;

	int l = arr->getArrayLength();
	CScriptVarPtr equal = c->constScriptVar(Undefined);
	for (i=0;i<l;i++) {
		equal = obj->mathsOp(arr->getArrayIndex(i), LEX_EQUAL);
		if(equal->toBoolean()) {
			removedIndices.push_back(i);
		}
	}
	if(removedIndices.size()) {
		vector<int>::iterator remove_it = removedIndices.begin();
		int next_remove = *remove_it;
		int next_insert = *remove_it++;
		for (i=next_remove;i<l;i++) {

			CScriptVarLinkPtr link = arr->findChild(int2string(i));
			if(i == next_remove) {
				if(link) arr->removeLink(link);
				if(remove_it != removedIndices.end())
					next_remove = *remove_it++;
			} else {
				if(link) {
					arr->setArrayIndex(next_insert++, link);
					arr->removeLink(link);
				}	
			}
		}
		arr->setArrayLength((uint32_t)next_insert);
	}
}

static void scArrayJoin(const CFunctionsScopePtr &c, void *data) {
	(void)data;
	CScriptVarPtr sepVar = c->getArgument("separator");
	string sep = ",";
	if (c->getArgumentsLength() >= 1 && sepVar && !sepVar->isUndefined())
		sep = sepVar->toString();
	CScriptVarPtr arr = c->getArgument("this");

	ostringstream sstr;
	int l = (int)arr->getArrayLength();
	for (int i=0;i<l;i++) {
		if (i>0) sstr << sep;
		CScriptVarPtr el = arr->getArrayIndex(i);
		if (el && !el->isUndefined() && !el->isNull())
			sstr << el->toString();
	}

	c->setReturnVar(c->newScriptVar(sstr.str()));
}

static bool hasArrayIndex(const CScriptVarPtr &arr, uint32_t i) {
	return arr->findChild(int2string(i));
}

static int64_t toInteger(const CScriptVarPtr &v) {
	CNumber n = v->toNumber();
	if (n.isNaN()) return 0;
	if (!n.isFinite())
		return n.isInfinity() > 0 ? (int64_t)0x7FFFFFFF : (int64_t)(-0x7FFFFFFF - 1);
	if (n.isDouble())
		return (int64_t)n.toDouble();
	return n.toInt64();
}

static uint32_t toRelativeIndex(int64_t rel, uint32_t len) {
	if (rel < 0) {
		int64_t r = (int64_t)len + rel;
		return r < 0 ? 0 : (uint32_t)r;
	}
	if (rel > (int64_t)len) return len;
	return (uint32_t)rel;
}

static CScriptVarPtr callArrayCallback(const CFunctionsScopePtr &c, const CScriptVarPtr &fn,
		const CScriptVarPtr &thisArg, const CScriptVarPtr &value, uint32_t idx, const CScriptVarPtr &arr) {
	if (!fn || !fn->isFunction())
		c->throwError(TypeError, "callback is not a function");
	vector<CScriptVarPtr> args;
	args.push_back(value);
	args.push_back(c->newScriptVar((int)idx));
	args.push_back(arr);
	return c->getContext()->callFunction(fn, args, thisArg ? thisArg : c->constScriptVar(Undefined));
}

static void scArrayIsArray(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr a = c->getArgument(0);
	c->setReturnVar(c->constScriptVar(a && a->isArray()));
}

static void scArrayPush(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t n = arr->getArrayLength();
	int argc = c->getArgumentsLength();
	for (int i = 0; i < argc; ++i)
		arr->setArrayIndex(n + (uint32_t)i, c->getArgument(i));
	c->setReturnVar(c->newScriptVar((int)arr->getArrayLength()));
}

static void scArrayPop(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t len = arr->getArrayLength();
	if (len == 0) {
		c->setReturnVar(c->constScriptVar(Undefined));
		return;
	}
	CScriptVarPtr last = arr->getArrayIndex(len - 1);
	arr->setArrayLength(len - 1);
	c->setReturnVar(last);
}

static void scArrayShift(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t len = arr->getArrayLength();
	if (len == 0) {
		c->setReturnVar(c->constScriptVar(Undefined));
		return;
	}
	CScriptVarPtr first = arr->getArrayIndex(0);
	for (uint32_t i = 1; i < len; ++i) {
		CScriptVarLinkPtr link = arr->findChild(int2string(i));
		if (link)
			arr->setArrayIndex(i - 1, link->getVarPtr());
		else {
			CScriptVarLinkPtr prev = arr->findChild(int2string(i - 1));
			if (prev) arr->removeLink(prev);
		}
	}
	CScriptVarLinkPtr tail = arr->findChild(int2string(len - 1));
	if (tail) arr->removeLink(tail);
	arr->setArrayLength(len - 1);
	c->setReturnVar(first);
}

static void scArrayUnshift(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	int argc = c->getArgumentsLength();
	uint32_t len = arr->getArrayLength();
	if (argc > 0) {
		for (int i = (int)len - 1; i >= 0; --i) {
			CScriptVarLinkPtr link = arr->findChild(int2string(i));
			if (link) {
				CScriptVarPtr val = link->getVarPtr();
				arr->removeLink(link);
				arr->setArrayIndex((uint32_t)i + (uint32_t)argc, val);
			} else {
				CScriptVarLinkPtr dest = arr->findChild(int2string((uint32_t)i + (uint32_t)argc));
				if (dest) arr->removeLink(dest);
			}
		}
		for (int i = 0; i < argc; ++i)
			arr->setArrayIndex((uint32_t)i, c->getArgument(i));
	}
	arr->setArrayLength(len + (uint32_t)argc);
	c->setReturnVar(c->newScriptVar((int)arr->getArrayLength()));
}

static void scArraySlice(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t len = arr->getArrayLength();
	uint32_t start = 0;
	uint32_t end = len;
	if (c->getArgumentsLength() >= 1)
		start = toRelativeIndex(toInteger(c->getArgument(0)), len);
	if (c->getArgumentsLength() >= 2)
		end = toRelativeIndex(toInteger(c->getArgument(1)), len);
	if (end < start) end = start;
	CScriptVarPtr out = c->newScriptVar(Array);
	uint32_t n = 0;
	for (uint32_t i = start; i < end; ++i) {
		if (hasArrayIndex(arr, i))
			out->setArrayIndex(n, arr->getArrayIndex(i));
		++n;
	}
	out->setArrayLength(n);
	c->setReturnVar(out);
}

static void scArrayConcat(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr out = c->newScriptVar(Array);
	uint32_t n = 0;
	int argc = c->getArgumentsLength();
	for (int a = -1; a < argc; ++a) {
		CScriptVarPtr src = (a < 0) ? arr : c->getArgument(a);
		if (src && src->isArray()) {
			uint32_t len = src->getArrayLength();
			for (uint32_t i = 0; i < len; ++i) {
				if (hasArrayIndex(src, i))
					out->setArrayIndex(n, src->getArrayIndex(i));
				++n;
			}
		} else {
			out->setArrayIndex(n++, src);
		}
	}
	out->setArrayLength(n);
	c->setReturnVar(out);
}

static void scArrayReverse(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t len = arr->getArrayLength();
	for (uint32_t i = 0; i < len / 2; ++i) {
		uint32_t j = len - 1 - i;
		bool hi = hasArrayIndex(arr, i);
		bool hj = hasArrayIndex(arr, j);
		CScriptVarPtr vi = hi ? arr->getArrayIndex(i) : CScriptVarPtr();
		CScriptVarPtr vj = hj ? arr->getArrayIndex(j) : CScriptVarPtr();
		CScriptVarLinkPtr li = arr->findChild(int2string(i));
		CScriptVarLinkPtr lj = arr->findChild(int2string(j));
		if (li) arr->removeLink(li);
		if (lj) arr->removeLink(lj);
		if (hj) arr->setArrayIndex(i, vj);
		if (hi) arr->setArrayIndex(j, vi);
	}
	arr->setArrayLength(len);
	c->setReturnVar(arr);
}

static int sortCompare(const CFunctionsScopePtr &c, const CScriptVarPtr &fn, const CScriptVarPtr &a, const CScriptVarPtr &b) {
	if (fn && fn->isFunction()) {
		vector<CScriptVarPtr> args;
		args.push_back(a);
		args.push_back(b);
		CScriptVarPtr r = c->getContext()->callFunction(fn, args, c->constScriptVar(Undefined));
		CNumber n = r->toNumber();
		if (n.isNaN() || n.isZero()) return 0;
		return n.sign() < 0 ? -1 : 1;
	}
	string sa = (a && !a->isUndefined() && !a->isNull()) ? a->toString() : "";
	string sb = (b && !b->isUndefined() && !b->isNull()) ? b->toString() : "";
	if (sa < sb) return -1;
	if (sa > sb) return 1;
	return 0;
}

static void scArraySort(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgumentsLength() >= 1 ? c->getArgument(0) : CScriptVarPtr();
	if (fn && !fn->isUndefined() && !fn->isFunction())
		c->throwError(TypeError, "comparefn is not a function");
	uint32_t len = arr->getArrayLength();
	vector<CScriptVarPtr> items;
	for (uint32_t i = 0; i < len; ++i)
		if (hasArrayIndex(arr, i))
			items.push_back(arr->getArrayIndex(i));
	for (size_t i = 1; i < items.size(); ++i) {
		CScriptVarPtr key = items[i];
		size_t j = i;
		while (j > 0 && sortCompare(c, fn, items[j - 1], key) > 0) {
			items[j] = items[j - 1];
			--j;
		}
		items[j] = key;
	}
	for (uint32_t i = 0; i < (uint32_t)items.size(); ++i)
		arr->setArrayIndex(i, items[i]);
	for (uint32_t i = (uint32_t)items.size(); i < len; ++i) {
		CScriptVarLinkPtr link = arr->findChild(int2string(i));
		if (link) arr->removeLink(link);
	}
	arr->setArrayLength(len);
	c->setReturnVar(arr);
}

static void scArraySplice(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	uint32_t len = arr->getArrayLength();
	int argc = c->getArgumentsLength();
	uint32_t start = argc >= 1 ? toRelativeIndex(toInteger(c->getArgument(0)), len) : 0;
	uint32_t deleteCount = len - start;
	if (argc >= 2) {
		int64_t dc = toInteger(c->getArgument(1));
		if (dc < 0) dc = 0;
		if (dc > (int64_t)(len - start)) dc = (int64_t)(len - start);
		deleteCount = (uint32_t)dc;
	}
	int insertCount = argc > 2 ? argc - 2 : 0;
	CScriptVarPtr removed = c->newScriptVar(Array);
	for (uint32_t i = 0; i < deleteCount; ++i) {
		if (hasArrayIndex(arr, start + i))
			removed->setArrayIndex(i, arr->getArrayIndex(start + i));
	}
	removed->setArrayLength(deleteCount);

	int64_t delta = (int64_t)insertCount - (int64_t)deleteCount;
	if (delta < 0) {
		for (uint32_t i = start + deleteCount; i < len; ++i) {
			CScriptVarLinkPtr link = arr->findChild(int2string(i));
			if (link) {
				CScriptVarPtr val = link->getVarPtr();
				arr->removeLink(link);
				arr->setArrayIndex((uint32_t)((int64_t)i + delta), val);
			} else {
				CScriptVarLinkPtr dest = arr->findChild(int2string((uint32_t)((int64_t)i + delta)));
				if (dest) arr->removeLink(dest);
			}
		}
		for (uint32_t i = (uint32_t)((int64_t)len + delta); i < len; ++i) {
			CScriptVarLinkPtr link = arr->findChild(int2string(i));
			if (link) arr->removeLink(link);
		}
	} else if (delta > 0) {
		for (int64_t i = (int64_t)len - 1; i >= (int64_t)start + (int64_t)deleteCount; --i) {
			CScriptVarLinkPtr link = arr->findChild(int2string((uint32_t)i));
			if (link) {
				CScriptVarPtr val = link->getVarPtr();
				arr->removeLink(link);
				arr->setArrayIndex((uint32_t)(i + delta), val);
			} else {
				CScriptVarLinkPtr dest = arr->findChild(int2string((uint32_t)(i + delta)));
				if (dest) arr->removeLink(dest);
			}
		}
	}
	for (int i = 0; i < insertCount; ++i)
		arr->setArrayIndex(start + (uint32_t)i, c->getArgument(2 + i));
	arr->setArrayLength((uint32_t)((int64_t)len + delta));
	c->setReturnVar(removed);
}

static void scArrayIndexOf(const CFunctionsScopePtr &c, void *data) {
	bool last = data != 0;
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr search = c->getArgument(0);
	uint32_t len = arr->getArrayLength();
	if (len == 0) {
		c->setReturnVar(c->newScriptVar(-1));
		return;
	}
	int64_t from;
	if (c->getArgumentsLength() >= 2)
		from = toInteger(c->getArgument(1));
	else
		from = last ? (int64_t)len - 1 : 0;
	int64_t idx;
	int64_t step;
	int64_t end;
	if (last) {
		if (from < 0) from = (int64_t)len + from;
		if (from < 0) {
			c->setReturnVar(c->newScriptVar(-1));
			return;
		}
		if (from >= (int64_t)len) from = (int64_t)len - 1;
		idx = from; step = -1; end = -1;
	} else {
		if (from < 0) from = (int64_t)len + from;
		if (from < 0) from = 0;
		if (from >= (int64_t)len) {
			c->setReturnVar(c->newScriptVar(-1));
			return;
		}
		idx = from; step = 1; end = (int64_t)len;
	}
	for (; idx != end; idx += step) {
		if (!hasArrayIndex(arr, (uint32_t)idx)) continue;
		CScriptVarPtr eq = search->mathsOp(arr->getArrayIndex((uint32_t)idx), LEX_TYPEEQUAL);
		if (eq && eq->toBoolean()) {
			c->setReturnVar(c->newScriptVar((int)idx));
			return;
		}
	}
	c->setReturnVar(c->newScriptVar(-1));
}

static void scArrayForEach(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgument(0);
	CScriptVarPtr thisArg = c->getArgumentsLength() >= 2 ? c->getArgument(1) : c->constScriptVar(Undefined);
	uint32_t len = arr->getArrayLength();
	for (uint32_t i = 0; i < len; ++i) {
		if (!hasArrayIndex(arr, i)) continue;
		callArrayCallback(c, fn, thisArg, arr->getArrayIndex(i), i, arr);
	}
	c->setReturnVar(c->constScriptVar(Undefined));
}

static void scArrayEvery(const CFunctionsScopePtr &c, void *data) {
	bool some = data != 0;
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgument(0);
	CScriptVarPtr thisArg = c->getArgumentsLength() >= 2 ? c->getArgument(1) : c->constScriptVar(Undefined);
	uint32_t len = arr->getArrayLength();
	for (uint32_t i = 0; i < len; ++i) {
		if (!hasArrayIndex(arr, i)) continue;
		CScriptVarPtr r = callArrayCallback(c, fn, thisArg, arr->getArrayIndex(i), i, arr);
		bool ok = r && r->toBoolean();
		if (some) {
			if (ok) { c->setReturnVar(c->constScriptVar(true)); return; }
		} else if (!ok) {
			c->setReturnVar(c->constScriptVar(false));
			return;
		}
	}
	c->setReturnVar(c->constScriptVar(some ? false : true));
}

static void scArrayMap(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgument(0);
	CScriptVarPtr thisArg = c->getArgumentsLength() >= 2 ? c->getArgument(1) : c->constScriptVar(Undefined);
	uint32_t len = arr->getArrayLength();
	CScriptVarPtr out = c->newScriptVar(Array);
	for (uint32_t i = 0; i < len; ++i) {
		if (!hasArrayIndex(arr, i)) continue;
		out->setArrayIndex(i, callArrayCallback(c, fn, thisArg, arr->getArrayIndex(i), i, arr));
	}
	out->setArrayLength(len);
	c->setReturnVar(out);
}

static void scArrayFilter(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgument(0);
	CScriptVarPtr thisArg = c->getArgumentsLength() >= 2 ? c->getArgument(1) : c->constScriptVar(Undefined);
	uint32_t len = arr->getArrayLength();
	CScriptVarPtr out = c->newScriptVar(Array);
	uint32_t n = 0;
	for (uint32_t i = 0; i < len; ++i) {
		if (!hasArrayIndex(arr, i)) continue;
		CScriptVarPtr val = arr->getArrayIndex(i);
		CScriptVarPtr r = callArrayCallback(c, fn, thisArg, val, i, arr);
		if (r && r->toBoolean())
			out->setArrayIndex(n++, val);
	}
	out->setArrayLength(n);
	c->setReturnVar(out);
}

static void scArrayReduce(const CFunctionsScopePtr &c, void *data) {
	bool fromRight = data != 0;
	CScriptVarPtr arr = c->getArgument("this");
	CScriptVarPtr fn = c->getArgument(0);
	if (!fn || !fn->isFunction())
		c->throwError(TypeError, "callback is not a function");
	uint32_t len = arr->getArrayLength();
	bool haveAcc = c->getArgumentsLength() >= 2;
	CScriptVarPtr acc = haveAcc ? c->getArgument(1) : CScriptVarPtr();
	if (fromRight) {
		for (int64_t i = (int64_t)len - 1; i >= 0; --i) {
			if (!hasArrayIndex(arr, (uint32_t)i)) continue;
			CScriptVarPtr val = arr->getArrayIndex((uint32_t)i);
			if (!haveAcc) { acc = val; haveAcc = true; continue; }
			vector<CScriptVarPtr> args;
			args.push_back(acc);
			args.push_back(val);
			args.push_back(c->newScriptVar((int)i));
			args.push_back(arr);
			acc = c->getContext()->callFunction(fn, args, c->constScriptVar(Undefined));
		}
	} else {
		for (uint32_t i = 0; i < len; ++i) {
			if (!hasArrayIndex(arr, i)) continue;
			CScriptVarPtr val = arr->getArrayIndex(i);
			if (!haveAcc) { acc = val; haveAcc = true; continue; }
			vector<CScriptVarPtr> args;
			args.push_back(acc);
			args.push_back(val);
			args.push_back(c->newScriptVar((int)i));
			args.push_back(arr);
			acc = c->getContext()->callFunction(fn, args, c->constScriptVar(Undefined));
		}
	}
	if (!haveAcc)
		c->throwError(TypeError, "Reduce of empty array with no initial value");
	c->setReturnVar(acc);
}

static void scObjectPropertyIsEnumerable(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr This = c->getArgument("this");
	string name = c->getArgument(0)->toString();
	CScriptVarLinkPtr prop = This->findChild(name);
	c->setReturnVar(c->constScriptVar(prop && prop->isEnumerable()));
}

static void scObjectIsPrototypeOf(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr This = c->getArgument("this");
	CScriptVarPtr V = c->getArgument(0);
	if (!V || (!V->isObject() && !V->isFunction())) {
		c->setReturnVar(c->constScriptVar(false));
		return;
	}
	CScriptVarPtr cur = V;
	for (int guard = 0; cur && guard < 1024; ++guard) {
		CScriptVarLinkPtr proto = cur->findChild(TINYJS___PROTO___VAR);
		if (!proto) break;
		CScriptVarPtr p = proto->getVarPtr();
		if (p.getVar() == This.getVar()) {
			c->setReturnVar(c->constScriptVar(true));
			return;
		}
		cur = p;
	}
	c->setReturnVar(c->constScriptVar(false));
}

static CScriptVarPtr binBytesOf(const CScriptVarPtr &obj) {
	if (!obj) return CScriptVarPtr();
	CScriptVarLinkPtr link = obj->findChild("__bytes");
	if (link) return link->getVarPtr();
	return CScriptVarPtr();
}

static uint8_t binByteAt(const CScriptVarPtr &bytes, uint32_t i) {
	if (!bytes) return 0;
	CScriptVarPtr v = bytes->getArrayIndex(i);
	if (!v) return 0;
	return (uint8_t)(v->toNumber().toInt32() & 0xFF);
}

static void binByteSet(const CFunctionsScopePtr &c, const CScriptVarPtr &bytes, uint32_t i, int v) {
	if (bytes)
		bytes->setArrayIndex(i, c->newScriptVar(v & 0xFF));
}

static CScriptVarPtr binMakeBytes(const CFunctionsScopePtr &c, uint32_t n) {
	CScriptVarPtr bytes = c->newScriptVar(Array);
	for (uint32_t i = 0; i < n; ++i)
		bytes->setArrayIndex(i, c->newScriptVar(0));
	bytes->setArrayLength(n);
	return bytes;
}

static CScriptVarPtr binMakeBuffer(const CFunctionsScopePtr &c, const CScriptVarPtr &bytes, uint32_t n) {
	CScriptVarPtr buf = c->newScriptVar(Object);
	buf->addChildOrReplace("byteLength", c->newScriptVar((int)n));
	buf->addChildOrReplace("__bytes", bytes);
	buf->addChildOrReplace("__isArrayBuffer", c->constScriptVar(true));
	return buf;
}

static void binAttachView(const CScriptVarPtr &view, const CScriptVarPtr &buf, const CScriptVarPtr &bytes, uint32_t offset, uint32_t length, const CFunctionsScopePtr &c) {
	view->addChildOrReplace("buffer", buf);
	view->addChildOrReplace("byteOffset", c->newScriptVar((int)offset));
	view->addChildOrReplace("byteLength", c->newScriptVar((int)length));
	view->addChildOrReplace("length", c->newScriptVar((int)length));
	view->addChildOrReplace("__bytes", bytes);
}

static bool binIsBuffer(const CScriptVarPtr &obj) {
	return obj && obj->findChild("__isArrayBuffer");
}

static void scArrayBuffer(const CFunctionsScopePtr &c, void *) {
	int32_t n = 0;
	if (c->getArgumentsLength() >= 1)
		n = c->getArgument(0)->toNumber().toInt32();
	if (n < 0) c->throwError(RangeError, "Invalid array buffer length");
	CScriptVarPtr bytes = binMakeBytes(c, (uint32_t)n);
	CScriptVarPtr self = c->getArgument("this");
	if (!self || self->isUndefined() || self->isNull())
		self = c->newScriptVar(Object);
	self->addChildOrReplace("byteLength", c->newScriptVar(n));
	self->addChildOrReplace("__bytes", bytes);
	self->addChildOrReplace("__isArrayBuffer", c->constScriptVar(true));
	c->setReturnVar(self);
}

static void scUint8Array(const CFunctionsScopePtr &c, void *) {
	int argc = c->getArgumentsLength();
	CScriptVarPtr bytes;
	CScriptVarPtr buf;
	uint32_t offset = 0, length = 0;
	if (argc < 1) {
		bytes = binMakeBytes(c, 0);
		buf = binMakeBuffer(c, bytes, 0);
	} else {
		CScriptVarPtr a0 = c->getArgument(0);
		if (binIsBuffer(a0)) {
			buf = a0;
			bytes = binBytesOf(a0);
			uint32_t cap = (uint32_t)a0->findChild("byteLength")->getVarPtr()->toNumber().toInt32();
			offset = argc >= 2 ? (uint32_t)c->getArgument(1)->toNumber().toInt32() : 0;
			length = argc >= 3 ? (uint32_t)c->getArgument(2)->toNumber().toInt32() : (cap > offset ? cap - offset : 0);
			if (offset + length > cap)
				c->throwError(RangeError, "Invalid typed array length");
			if (offset == 0 && length == cap)
				; // share
			else {
				CScriptVarPtr slice = binMakeBytes(c, length);
				for (uint32_t i = 0; i < length; ++i)
					binByteSet(c, slice, i, binByteAt(bytes, offset + i));
				bytes = slice;
				buf = binMakeBuffer(c, bytes, length);
				offset = 0;
			}
		} else if (a0->isArray() || a0->findChild("length")) {
			length = a0->getArrayLength();
			if (a0->findChild("length") && !a0->isArray())
				length = (uint32_t)a0->findChild("length")->getVarPtr()->toNumber().toInt32();
			bytes = binMakeBytes(c, length);
			for (uint32_t i = 0; i < length; ++i)
				binByteSet(c, bytes, i, binByteAt(a0, i));
			buf = binMakeBuffer(c, bytes, length);
		} else {
			int32_t n = a0->toNumber().toInt32();
			if (n < 0) c->throwError(RangeError, "Invalid typed array length");
			length = (uint32_t)n;
			bytes = binMakeBytes(c, length);
			buf = binMakeBuffer(c, bytes, length);
		}
	}
	binAttachView(bytes, buf, bytes, offset, length ? length : bytes->getArrayLength(), c);
	if (!length) length = bytes->getArrayLength();
	bytes->setArrayLength(length);
	CScriptVarLinkPtr ctor = c->getContext()->getRoot()->findChild("Uint8Array");
	if (ctor) {
		CScriptVarLinkPtr proto = ctor->getVarPtr()->findChild("prototype");
		if (proto)
			bytes->addChildOrReplace(TINYJS___PROTO___VAR, proto);
	}
	c->setReturnVar(bytes);
}

static void scUint8ArraySet(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr self = c->getArgument("this");
	CScriptVarPtr bytes = binBytesOf(self);
	if (!bytes) bytes = self;
	uint32_t off = 0;
	if (c->getArgumentsLength() >= 2)
		off = (uint32_t)c->getArgument(1)->toNumber().toInt32();
	CScriptVarPtr src = c->getArgument(0);
	uint32_t n = src->getArrayLength();
	if (src->findChild("length") && !src->isArray())
		n = (uint32_t)src->findChild("length")->getVarPtr()->toNumber().toInt32();
	for (uint32_t i = 0; i < n; ++i)
		binByteSet(c, bytes, off + i, binByteAt(src, i));
	c->setReturnVar(c->constScriptVar(Undefined));
}

static void scDataView(const CFunctionsScopePtr &c, void *) {
	if (c->getArgumentsLength() < 1)
		c->throwError(TypeError, "DataView requires an ArrayBuffer");
	CScriptVarPtr buf = c->getArgument(0);
	if (!binIsBuffer(buf))
		c->throwError(TypeError, "First argument to DataView constructor must be an ArrayBuffer");
	uint32_t cap = (uint32_t)buf->findChild("byteLength")->getVarPtr()->toNumber().toInt32();
	uint32_t offset = 0, length = cap;
	if (c->getArgumentsLength() >= 2)
		offset = (uint32_t)c->getArgument(1)->toNumber().toInt32();
	if (c->getArgumentsLength() >= 3)
		length = (uint32_t)c->getArgument(2)->toNumber().toInt32();
	else
		length = cap > offset ? cap - offset : 0;
	if (offset + length > cap)
		c->throwError(RangeError, "Invalid DataView length");
	CScriptVarPtr self = c->getArgument("this");
	if (!self || self->isUndefined() || self->isNull())
		self = c->newScriptVar(Object);
	self->addChildOrReplace("buffer", buf);
	self->addChildOrReplace("byteOffset", c->newScriptVar((int)offset));
	self->addChildOrReplace("byteLength", c->newScriptVar((int)length));
	c->setReturnVar(self);
}

static bool dvBounds(const CFunctionsScopePtr &c, uint32_t need, uint32_t &off, CScriptVarPtr &bytes) {
	CScriptVarPtr self = c->getArgument("this");
	CScriptVarPtr bufLink = self->findChild("buffer") ? self->findChild("buffer")->getVarPtr() : CScriptVarPtr();
	bytes = binBytesOf(bufLink);
	if (!bytes) { c->throwError(TypeError, "DataView is detached"); return false; }
	uint32_t base = self->findChild("byteOffset") ? (uint32_t)self->findChild("byteOffset")->getVarPtr()->toNumber().toInt32() : 0;
	uint32_t span = self->findChild("byteLength") ? (uint32_t)self->findChild("byteLength")->getVarPtr()->toNumber().toInt32() : 0;
	off = base;
	if (c->getArgumentsLength() >= 1)
		off = base + (uint32_t)c->getArgument(0)->toNumber().toInt32();
	if ((off - base) + need > span)
		c->throwError(RangeError, "Offset is outside the bounds of the DataView");
	return true;
}

static bool dvLE(const CFunctionsScopePtr &c, int endianArg) {
	if (c->getArgumentsLength() > endianArg)
		return c->getArgument(endianArg)->toBoolean();
	return false;
}

static void scDataViewGetUint8(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 1, off, bytes)) return;
	c->setReturnVar(c->newScriptVar((int)binByteAt(bytes, off)));
}
static void scDataViewGetInt8(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 1, off, bytes)) return;
	int v = binByteAt(bytes, off);
	if (v > 127) v -= 256;
	c->setReturnVar(c->newScriptVar(v));
}
static void scDataViewSetUint8(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 1, off, bytes)) return;
	int v = c->getArgumentsLength() >= 2 ? c->getArgument(1)->toNumber().toInt32() : 0;
	binByteSet(c, bytes, off, v);
	c->setReturnVar(c->constScriptVar(Undefined));
}
static void scDataViewSetInt8(const CFunctionsScopePtr &c, void *) { scDataViewSetUint8(c, 0); }

static uint32_t dvReadU(const CScriptVarPtr &bytes, uint32_t off, int width, bool le) {
	uint32_t v = 0;
	for (int i = 0; i < width; ++i) {
		uint8_t b = binByteAt(bytes, off + i);
		if (le) v |= ((uint32_t)b) << (8 * i);
		else v = (v << 8) | b;
	}
	return v;
}
static void dvWriteU(const CFunctionsScopePtr &c, const CScriptVarPtr &bytes, uint32_t off, int width, uint32_t v, bool le) {
	for (int i = 0; i < width; ++i) {
		int shift = le ? (8 * i) : (8 * (width - 1 - i));
		binByteSet(c, bytes, off + i, (int)((v >> shift) & 0xFF));
	}
}

static void scDataViewGetUint16(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 2, off, bytes)) return;
	c->setReturnVar(c->newScriptVar((int)dvReadU(bytes, off, 2, dvLE(c, 1))));
}
static void scDataViewGetInt16(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 2, off, bytes)) return;
	int v = (int)dvReadU(bytes, off, 2, dvLE(c, 1));
	if (v > 32767) v -= 65536;
	c->setReturnVar(c->newScriptVar(v));
}
static void scDataViewSetUint16(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 2, off, bytes)) return;
	uint32_t v = c->getArgumentsLength() >= 2 ? (uint32_t)c->getArgument(1)->toNumber().toInt32() : 0;
	dvWriteU(c, bytes, off, 2, v, dvLE(c, 2));
	c->setReturnVar(c->constScriptVar(Undefined));
}
static void scDataViewSetInt16(const CFunctionsScopePtr &c, void *) { scDataViewSetUint16(c, 0); }

static void scDataViewGetUint32(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 4, off, bytes)) return;
	uint32_t v = dvReadU(bytes, off, 4, dvLE(c, 1));
	c->setReturnVar(c->newScriptVar((int64_t)(uint64_t)v));
}
static void scDataViewGetInt32(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 4, off, bytes)) return;
	int32_t v = (int32_t)dvReadU(bytes, off, 4, dvLE(c, 1));
	c->setReturnVar(c->newScriptVar((int)v));
}
static void scDataViewSetUint32(const CFunctionsScopePtr &c, void *) {
	uint32_t off; CScriptVarPtr bytes;
	if (!dvBounds(c, 4, off, bytes)) return;
	uint32_t v = 0;
	if (c->getArgumentsLength() >= 2)
		v = c->getArgument(1)->toNumber().toUInt32();
	dvWriteU(c, bytes, off, 4, v, dvLE(c, 2));
	c->setReturnVar(c->constScriptVar(Undefined));
}
static void scDataViewSetInt32(const CFunctionsScopePtr &c, void *) { scDataViewSetUint32(c, 0); }

static CScriptVarPtr mapKeys(const CScriptVarPtr &m) {
	CScriptVarLinkPtr k = m->findChild("__mk");
	return k ? k->getVarPtr() : CScriptVarPtr();
}
static CScriptVarPtr mapVals(const CScriptVarPtr &m) {
	CScriptVarLinkPtr v = m->findChild("__mv");
	return v ? v->getVarPtr() : CScriptVarPtr();
}
static int mapIndexOf(const CScriptVarPtr &keys, const CScriptVarPtr &key) {
	if (!keys || !key) return -1;
	uint32_t n = keys->getArrayLength();
	for (uint32_t i = 0; i < n; ++i) {
		CScriptVarPtr k = keys->getArrayIndex(i);
		if (k && k->mathsOp(key, LEX_TYPEEQUAL)->toBoolean())
			return (int)i;
	}
	return -1;
}

static void scMap(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr self = c->getArgument("this");
	if (!self || self->isUndefined() || self->isNull())
		self = c->newScriptVar(Object);
	self->addChildOrReplace("__mk", c->newScriptVar(Array));
	self->addChildOrReplace("__mv", c->newScriptVar(Array));
	self->addChildOrReplace("size", c->newScriptVar(0));
	c->setReturnVar(self);
}
static void scMapSet(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr self = c->getArgument("this");
	CScriptVarPtr keys = mapKeys(self), vals = mapVals(self);
	if (!keys || !vals) c->throwError(TypeError, "Map.prototype.set called on incompatible Object");
	CScriptVarPtr key = c->getArgument(0);
	CScriptVarPtr val = c->getArgumentsLength() >= 2 ? c->getArgument(1) : c->constScriptVar(Undefined);
	int idx = mapIndexOf(keys, key);
	if (idx < 0) {
		uint32_t n = keys->getArrayLength();
		keys->setArrayIndex(n, key);
		vals->setArrayIndex(n, val);
		keys->setArrayLength(n + 1);
		vals->setArrayLength(n + 1);
		self->addChildOrReplace("size", c->newScriptVar((int)(n + 1)));
	} else
		vals->setArrayIndex((uint32_t)idx, val);
	c->setReturnVar(self);
}
static void scMapGet(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr keys = mapKeys(c->getArgument("this")), vals = mapVals(c->getArgument("this"));
	if (!keys || !vals) c->throwError(TypeError, "Map.prototype.get called on incompatible Object");
	int idx = mapIndexOf(keys, c->getArgument(0));
	c->setReturnVar(idx < 0 ? c->constScriptVar(Undefined) : vals->getArrayIndex((uint32_t)idx));
}
static void scMapHas(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr keys = mapKeys(c->getArgument("this"));
	if (!keys) c->throwError(TypeError, "Map.prototype.has called on incompatible Object");
	c->setReturnVar(c->constScriptVar(mapIndexOf(keys, c->getArgument(0)) >= 0));
}
static void scMapDelete(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr self = c->getArgument("this");
	CScriptVarPtr keys = mapKeys(self), vals = mapVals(self);
	if (!keys || !vals) c->throwError(TypeError, "Map.prototype.delete called on incompatible Object");
	int idx = mapIndexOf(keys, c->getArgument(0));
	if (idx < 0) { c->setReturnVar(c->constScriptVar(false)); return; }
	uint32_t n = keys->getArrayLength();
	for (uint32_t i = (uint32_t)idx; i + 1 < n; ++i) {
		keys->setArrayIndex(i, keys->getArrayIndex(i + 1));
		vals->setArrayIndex(i, vals->getArrayIndex(i + 1));
	}
	keys->setArrayLength(n - 1);
	vals->setArrayLength(n - 1);
	self->addChildOrReplace("size", c->newScriptVar((int)(n - 1)));
	c->setReturnVar(c->constScriptVar(true));
}
static void scMapClear(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr self = c->getArgument("this");
	self->addChildOrReplace("__mk", c->newScriptVar(Array));
	self->addChildOrReplace("__mv", c->newScriptVar(Array));
	self->addChildOrReplace("size", c->newScriptVar(0));
	c->setReturnVar(c->constScriptVar(Undefined));
}
static void scMapIterNext(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr it = c->getArgument("this");
	CScriptVarPtr map = it->findChild("__map") ? it->findChild("__map")->getVarPtr() : CScriptVarPtr();
	CScriptVarPtr keys = mapKeys(map), vals = mapVals(map);
	int i = it->findChild("__i") ? it->findChild("__i")->getVarPtr()->toNumber().toInt32() : 0;
	if (!keys || i >= (int)keys->getArrayLength())
		throw c->constScriptVar(StopIteration);
	it->addChildOrReplace("__i", c->newScriptVar(i + 1));
	CScriptVarPtr pair = c->newScriptVar(Array);
	pair->setArrayIndex(0, keys->getArrayIndex((uint32_t)i));
	pair->setArrayIndex(1, vals->getArrayIndex((uint32_t)i));
	pair->setArrayLength(2);
	c->setReturnVar(pair);
}
static void scMapIterator(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr it = c->newScriptVar(Object);
	it->addChildOrReplace("__map", c->getArgument("this"));
	it->addChildOrReplace("__i", c->newScriptVar(0));
	it->addChildOrReplace("next", c->getContext()->newScriptVar(scMapIterNext, (void*)0, "MapIterator.next"));
	c->setReturnVar(it);
}

static void scBigInt(const CFunctionsScopePtr &c, void *) {
	if (c->getArgumentsLength() < 1)
		c->throwError(TypeError, "Cannot convert undefined to a BigInt");
	CScriptVarPtr a = c->getArgument(0);
	CNumber n;
	if (a->isString()) {
		n.parseInt(a->toString().c_str(), 0);
	} else {
		n = a->toNumber();
		if (n.isNaN() || n.isInfinity() || n.isDouble()) {
			double d = n.toDouble();
			if (n.isNaN() || n.isInfinity() || d != floor(d))
				c->throwError(RangeError, "The number " + a->toString() + " cannot be converted to a BigInt");
			n = CNumber((int64_t)d);
		}
	}
	n.setBigInt(true);
	c->setReturnVar(c->newScriptVar(n));
}

// ----------------------------------------------- Register Functions
void registerFunctions(CTinyJS *tinyJS) {
}
extern "C" void _registerFunctions(CTinyJS *tinyJS) {
	tinyJS->addNative("function trace()", scTrace, tinyJS, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Object.prototype.dump()", scObjectDump, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Object.prototype.clone()", scObjectClone, 0, SCRIPTVARLINK_BUILDINDEFAULT);

//	tinyJS->addNative("function Integer.valueOf(str)", scIntegerValueOf, 0, SCRIPTVARLINK_BUILDINDEFAULT); // value of a single character
	tinyJS->addNative("function JSON.stringify(obj, replacer)", scJSONStringify, 0, SCRIPTVARLINK_BUILDINDEFAULT); // replacer ignored
	tinyJS->addNative("function Object.prototype.propertyIsEnumerable(prop)", scObjectPropertyIsEnumerable, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Object.prototype.isPrototypeOf(obj)", scObjectIsPrototypeOf, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.isArray(arg)", scArrayIsArray, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.contains(obj)", scArrayContains, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.remove(obj)", scArrayRemove, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.join(separator)", scArrayJoin, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.push()", scArrayPush, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.pop()", scArrayPop, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.shift()", scArrayShift, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.unshift()", scArrayUnshift, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.slice(start, end)", scArraySlice, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.concat()", scArrayConcat, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.reverse()", scArrayReverse, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.sort(comparefn)", scArraySort, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.splice(start, deleteCount)", scArraySplice, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.indexOf(searchElement, fromIndex)", scArrayIndexOf, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.lastIndexOf(searchElement, fromIndex)", scArrayIndexOf, (void*)1, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.forEach(callback, thisArg)", scArrayForEach, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.every(callback, thisArg)", scArrayEvery, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.some(callback, thisArg)", scArrayEvery, (void*)1, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.map(callback, thisArg)", scArrayMap, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.filter(callback, thisArg)", scArrayFilter, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.reduce(callback, initial)", scArrayReduce, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Array.prototype.reduceRight(callback, initial)", scArrayReduce, (void*)1, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function ArrayBuffer(length)", scArrayBuffer, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Uint8Array(arg, byteOffset, length)", scUint8Array, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Uint8Array.prototype.set(src, offset)", scUint8ArraySet, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView(buffer, byteOffset, byteLength)", scDataView, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getUint8(byteOffset)", scDataViewGetUint8, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getInt8(byteOffset)", scDataViewGetInt8, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setUint8(byteOffset, value)", scDataViewSetUint8, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setInt8(byteOffset, value)", scDataViewSetInt8, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getUint16(byteOffset, littleEndian)", scDataViewGetUint16, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getInt16(byteOffset, littleEndian)", scDataViewGetInt16, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setUint16(byteOffset, value, littleEndian)", scDataViewSetUint16, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setInt16(byteOffset, value, littleEndian)", scDataViewSetInt16, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getUint32(byteOffset, littleEndian)", scDataViewGetUint32, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.getInt32(byteOffset, littleEndian)", scDataViewGetInt32, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setUint32(byteOffset, value, littleEndian)", scDataViewSetUint32, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function DataView.prototype.setInt32(byteOffset, value, littleEndian)", scDataViewSetInt32, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function BigInt(value)", scBigInt, 0, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Map()", scMap, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.set(key, value)", scMapSet, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.get(key)", scMapGet, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.has(key)", scMapHas, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.delete(key)", scMapDelete, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.clear()", scMapClear, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Map.prototype.__iterator__()", scMapIterator, 0, SCRIPTVARLINK_BUILDINDEFAULT);
}

