#include "TinyJS.h"
#include "TinyJS_Date.h"
#include <chrono>
#include <cstring>

using namespace std;
using tinyjs::date::DateTimeToTimestampMs;
using tinyjs::date::Iso8601ToTimestampMs;
using tinyjs::date::TimestampMsToIso8601;
using tinyjs::date::TimestampMsToTm;

static const char *kDateMs = "__date_ms__";

enum DateField {
	dfTime = 0,
	dfYear,
	dfMonth,
	dfDate,
	dfDay,
	dfHours,
	dfMinutes,
	dfSeconds,
	dfMs
};

static int64_t nowMs() {
	return chrono::duration_cast<chrono::milliseconds>(
		chrono::system_clock::now().time_since_epoch()).count();
}

static CNumber msNumber(int64_t ms) {
	CNumber n((int64_t)ms);
	n.setBigInt(true);
	return n;
}

static CScriptVarPtr datePrototypeOf(const CFunctionsScopePtr &c) {
	CScriptVarLinkPtr Date = c->getContext()->getRoot()->findChild("Date");
	if (!Date) return CScriptVarPtr();
	CScriptVarLinkPtr proto = Date->getVarPtr()->findChild(TINYJS_PROTOTYPE_CLASS);
	return proto ? proto->getVarPtr() : CScriptVarPtr();
}

static CScriptVarPtr makeDateObject(const CFunctionsScopePtr &c, int64_t ms, bool valid) {
	CScriptVarPtr proto = datePrototypeOf(c);
	CScriptVarPtr obj = proto ? c->newScriptVar(proto, string("Date"))
		: c->newScriptVar(Object);
	if (valid)
		obj->addChild(kDateMs, c->newScriptVar(msNumber(ms)), 0);
	else
		obj->addChild(kDateMs, c->constScriptVar(NaN), 0);
	return obj;
}

static bool readDateMs(const CScriptVarPtr &obj, int64_t &ms) {
	if (!obj) return false;
	CScriptVarLinkPtr link = obj->findChild(kDateMs);
	if (!link) return false;
	CNumber n = link->toNumber();
	if (!n.isFinite()) return false;
	ms = n.toInt64();
	return true;
}

static bool thisDateMs(const CFunctionsScopePtr &c, int64_t &ms) {
	return readDateMs(c->getArgument("this"), ms);
}

static void writeDateMs(const CScriptVarPtr &obj, const CFunctionsScopePtr &c, int64_t ms, bool valid) {
	if (!obj) return;
	if (valid)
		obj->addChildOrReplace(kDateMs, c->newScriptVar(msNumber(ms)), 0);
	else
		obj->addChildOrReplace(kDateMs, c->constScriptVar(NaN), 0);
}

static int64_t normalizeYear(int64_t y) {
	if (y >= 0 && y <= 99) return 1900 + y;
	return y;
}

static bool parseDateValue(const CFunctionsScopePtr &c, const CScriptVarPtr &v, int64_t &ms) {
	if (!v || v->isUndefined() || v->isNull()) return false;
	if (v->isNumber()) {
		CNumber n = v->toNumber();
		if (!n.isFinite()) return false;
		ms = n.toInt64();
		return true;
	}
	if (readDateMs(v, ms))
		return true;
	string s = v->toString();
	if (s.empty()) return false;
	bool digits = true;
	for (size_t i = 0; i < s.size(); ++i) {
		char ch = s[i];
		if (ch == '-' && i == 0) continue;
		if (ch < '0' || ch > '9') { digits = false; break; }
	}
	if (digits) {
		CNumber n;
		n.parseFloat(s.c_str());
		if (!n.isFinite()) return false;
		ms = n.toInt64();
		return true;
	}
	if (s.size() >= 4 && s[0] >= '0' && s[0] <= '9') {
		ms = Iso8601ToTimestampMs(s);
		return true;
	}
	return false;
}

static int64_t argsToMs(const CFunctionsScopePtr &c, int start) {
	int argc = c->getArgumentsLength();
	int64_t y = normalizeYear(c->getArgument(start)->toNumber().toInt64());
	int64_t month = (argc > start + 1) ? c->getArgument(start + 1)->toNumber().toInt64() : 0;
	int64_t day = (argc > start + 2) ? c->getArgument(start + 2)->toNumber().toInt64() : 1;
	int64_t hour = (argc > start + 3) ? c->getArgument(start + 3)->toNumber().toInt64() : 0;
	int64_t min = (argc > start + 4) ? c->getArgument(start + 4)->toNumber().toInt64() : 0;
	int64_t sec = (argc > start + 5) ? c->getArgument(start + 5)->toNumber().toInt64() : 0;
	int64_t milli = (argc > start + 6) ? c->getArgument(start + 6)->toNumber().toInt64() : 0;
	if (month < 0) month = 0;
	if (month > 11) month = 11;
	if (day < 1) day = 1;
	return (int64_t)DateTimeToTimestampMs(
		(uint32_t)day, (uint32_t)month, (uint32_t)y,
		(uint32_t)hour, (uint32_t)min, (uint32_t)sec, (uint32_t)milli);
}

static void scDate(const CFunctionsScopePtr &c, void *data) {
	bool asCtor = data != 0;
	int argc = c->getArgumentsLength();
	int64_t ms = 0;
	bool valid = true;
	if (argc == 0) {
		ms = nowMs();
	} else if (argc == 1) {
		valid = parseDateValue(c, c->getArgument(0), ms);
	} else {
		ms = argsToMs(c, 0);
	}
	if (!asCtor) {
		c->setReturnVar(c->newScriptVar(valid ? TimestampMsToIso8601(ms, 0) : "Invalid Date"));
		return;
	}
	c->setReturnVar(makeDateObject(c, ms, valid));
}

static void scDateNow(const CFunctionsScopePtr &c, void *) {
	c->setReturnVar(c->newScriptVar(msNumber(nowMs())));
}

static void scDateParse(const CFunctionsScopePtr &c, void *) {
	int64_t ms = 0;
	if (!parseDateValue(c, c->getArgument(0), ms))
		c->setReturnVar(c->constScriptVar(NaN));
	else
		c->setReturnVar(c->newScriptVar(msNumber(ms)));
}

static void scDateUTC(const CFunctionsScopePtr &c, void *) {
	if (c->getArgumentsLength() < 2) {
		c->setReturnVar(c->constScriptVar(NaN));
		return;
	}
	c->setReturnVar(c->newScriptVar(msNumber(argsToMs(c, 0))));
}

static void scDateGet(const CFunctionsScopePtr &c, void *data) {
	int64_t ms = 0;
	if (!thisDateMs(c, ms)) {
		c->setReturnVar(c->constScriptVar(NaN));
		return;
	}
	intptr_t field = (intptr_t)data;
	if (field == dfTime) {
		c->setReturnVar(c->newScriptVar(msNumber(ms)));
		return;
	}
	struct tm t;
	uint32_t milli = 0;
	TimestampMsToTm((uint64_t)ms, t, milli);
	int v = 0;
	switch (field) {
	case dfYear: v = t.tm_year; break;
	case dfMonth: v = t.tm_mon; break;
	case dfDate: v = t.tm_mday; break;
	case dfDay: v = t.tm_wday; break;
	case dfHours: v = t.tm_hour; break;
	case dfMinutes: v = t.tm_min; break;
	case dfSeconds: v = t.tm_sec; break;
	case dfMs: v = (int)milli; break;
	default: break;
	}
	c->setReturnVar(c->newScriptVar(v));
}

static void scDateSetTime(const CFunctionsScopePtr &c, void *) {
	CScriptVarPtr This = c->getArgument("this");
	int64_t ms = 0;
	bool valid = parseDateValue(c, c->getArgument(0), ms);
	writeDateMs(This, c, ms, valid);
	if (valid)
		c->setReturnVar(c->newScriptVar(msNumber(ms)));
	else
		c->setReturnVar(c->constScriptVar(NaN));
}

static void scDateSetField(const CFunctionsScopePtr &c, void *data) {
	CScriptVarPtr This = c->getArgument("this");
	int64_t ms = 0;
	if (!readDateMs(This, ms)) {
		c->setReturnVar(c->constScriptVar(NaN));
		return;
	}
	struct tm t;
	uint32_t milli = 0;
	TimestampMsToTm((uint64_t)ms, t, milli);
	int argc = c->getArgumentsLength();
	intptr_t field = (intptr_t)data;
	auto num = [&](int i) { return c->getArgument(i)->toNumber().toInt64(); };
	switch (field) {
	case dfYear:
		t.tm_year = (int)normalizeYear(num(0));
		if (argc > 1) t.tm_mon = (int)num(1);
		if (argc > 2) t.tm_mday = (int)num(2);
		break;
	case dfMonth:
		t.tm_mon = (int)num(0);
		if (argc > 1) t.tm_mday = (int)num(1);
		break;
	case dfDate:
		t.tm_mday = (int)num(0);
		break;
	case dfHours:
		t.tm_hour = (int)num(0);
		if (argc > 1) t.tm_min = (int)num(1);
		if (argc > 2) t.tm_sec = (int)num(2);
		if (argc > 3) milli = (uint32_t)num(3);
		break;
	case dfMinutes:
		t.tm_min = (int)num(0);
		if (argc > 1) t.tm_sec = (int)num(1);
		if (argc > 2) milli = (uint32_t)num(2);
		break;
	case dfSeconds:
		t.tm_sec = (int)num(0);
		if (argc > 1) milli = (uint32_t)num(1);
		break;
	case dfMs:
		milli = (uint32_t)num(0);
		break;
	default:
		break;
	}
	if (t.tm_mon < 0) t.tm_mon = 0;
	if (t.tm_mon > 11) t.tm_mon = 11;
	if (t.tm_mday < 1) t.tm_mday = 1;
	int64_t next = (int64_t)DateTimeToTimestampMs(
		(uint32_t)t.tm_mday, (uint32_t)t.tm_mon, (uint32_t)t.tm_year,
		(uint32_t)t.tm_hour, (uint32_t)t.tm_min, (uint32_t)t.tm_sec, milli);
	writeDateMs(This, c, next, true);
	c->setReturnVar(c->newScriptVar(msNumber(next)));
}

static void scDateToISO(const CFunctionsScopePtr &c, void *) {
	int64_t ms = 0;
	if (!thisDateMs(c, ms)) {
		c->throwError(RangeError, "Invalid time value");
		return;
	}
	c->setReturnVar(c->newScriptVar(TimestampMsToIso8601(ms, 0)));
}

static void scDateToString(const CFunctionsScopePtr &c, void *) {
	int64_t ms = 0;
	if (!thisDateMs(c, ms)) {
		c->setReturnVar(c->newScriptVar(string("Invalid Date")));
		return;
	}
	c->setReturnVar(c->newScriptVar(TimestampMsToIso8601(ms, 0)));
}

extern "C" void _registerDateFunctions(CTinyJS *tinyJS) {
	tinyJS->addNative("function Date()", scDate, 0, SCRIPTVARLINK_CONSTANT);
	tinyJS->addNative("function Date.__constructor__()", scDate, (void*)1, SCRIPTVARLINK_CONSTANT);
	tinyJS->addNative("function Date.now()", scDateNow, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.parse(value)", scDateParse, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.UTC(year, month)", scDateUTC, 0, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Date.prototype.getTime()", scDateGet, (void*)dfTime, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.valueOf()", scDateGet, (void*)dfTime, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setTime(ms)", scDateSetTime, 0, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Date.prototype.getUTCFullYear()", scDateGet, (void*)dfYear, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCMonth()", scDateGet, (void*)dfMonth, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCDate()", scDateGet, (void*)dfDate, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCDay()", scDateGet, (void*)dfDay, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCHours()", scDateGet, (void*)dfHours, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCMinutes()", scDateGet, (void*)dfMinutes, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCSeconds()", scDateGet, (void*)dfSeconds, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getUTCMilliseconds()", scDateGet, (void*)dfMs, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Date.prototype.getFullYear()", scDateGet, (void*)dfYear, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getMonth()", scDateGet, (void*)dfMonth, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getDate()", scDateGet, (void*)dfDate, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getDay()", scDateGet, (void*)dfDay, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getHours()", scDateGet, (void*)dfHours, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getMinutes()", scDateGet, (void*)dfMinutes, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getSeconds()", scDateGet, (void*)dfSeconds, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.getMilliseconds()", scDateGet, (void*)dfMs, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Date.prototype.setUTCFullYear(year)", scDateSetField, (void*)dfYear, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCMonth(month)", scDateSetField, (void*)dfMonth, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCDate(date)", scDateSetField, (void*)dfDate, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCHours(hours)", scDateSetField, (void*)dfHours, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCMinutes(min)", scDateSetField, (void*)dfMinutes, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCSeconds(sec)", scDateSetField, (void*)dfSeconds, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setUTCMilliseconds(ms)", scDateSetField, (void*)dfMs, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setFullYear(year)", scDateSetField, (void*)dfYear, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setMonth(month)", scDateSetField, (void*)dfMonth, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setDate(date)", scDateSetField, (void*)dfDate, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setHours(hours)", scDateSetField, (void*)dfHours, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setMinutes(min)", scDateSetField, (void*)dfMinutes, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setSeconds(sec)", scDateSetField, (void*)dfSeconds, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.setMilliseconds(ms)", scDateSetField, (void*)dfMs, SCRIPTVARLINK_BUILDINDEFAULT);

	tinyJS->addNative("function Date.prototype.toISOString()", scDateToISO, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.toJSON()", scDateToISO, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.toString()", scDateToString, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.toUTCString()", scDateToString, 0, SCRIPTVARLINK_BUILDINDEFAULT);
	tinyJS->addNative("function Date.prototype.toGMTString()", scDateToString, 0, SCRIPTVARLINK_BUILDINDEFAULT);
}
