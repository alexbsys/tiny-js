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
}

