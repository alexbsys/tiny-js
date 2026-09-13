/*
 * TinyJS Date — calendar helpers pulled from cmfdate.h, plus Date registration.
 *
 * Conversion routines originated in cmf/src/modules/tools/tools_common/include/cmfdate.h
 * (cmf::date). Copied here so the engine does not depend on CMF headers.
 */

#ifndef TINYJS_DATE_H
#define TINYJS_DATE_H

#include <cstdint>
#include <ctime>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>

class CTinyJS;

extern "C" void _registerDateFunctions(CTinyJS *tinyJS);

namespace tinyjs {
namespace date {

enum {
	kSecondsInMinute = 60,
	kSecondsInHour = 3600,
	kSecondsInDay = 86400,
	kMinutesInHour = 60,
	kHoursInDay = 24,
	kMonthsInYear = 12,
	kDaysInWeek = 7,
	kDaysInYear = 365
};

enum { SUN = 0, MON, TUS, WED, THU, FRI, SAT };

inline uint32_t TimestampMsToWeekDay(int64_t timestamp_ms) {
	return (uint32_t)(((timestamp_ms / 1000) / kSecondsInDay + THU) % kDaysInWeek);
}

inline uint64_t DateTimeToTimestampMs(
	uint32_t day, uint32_t month, uint32_t year,
	uint32_t hour = 0, uint32_t minute = 0, uint32_t second = 0, uint32_t millisecond = 0)
{
	static const long kBiasYear = 1900;
	static const long kBiasDays = 25567;
	const long lmos[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };
	const long mos[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	long normalized_year = (long)year - kBiasYear;
	long _mon = (long)month;
	if (_mon < 0) _mon = 0;
	if (_mon > 11) _mon = 11;

	int64_t total_days = (((normalized_year - 1) / 4) + ((((normalized_year) & 03) || ((normalized_year) == 0)) ? mos[_mon] : lmos[_mon])) - 1;
	total_days += kDaysInYear * normalized_year;
	total_days += day;
	total_days -= kBiasDays;

	uint64_t total_secs = (uint64_t)kSecondsInHour * hour;
	total_secs += (uint64_t)kSecondsInMinute * minute;
	total_secs += second;
	total_secs += (uint64_t)total_days * kSecondsInDay;
	return (total_secs * 1000ULL) + millisecond;
}

inline uint64_t TmToTimestampMs(const struct tm &dt, uint32_t ms) {
	return DateTimeToTimestampMs(dt.tm_mday, dt.tm_mon, dt.tm_year, dt.tm_hour, dt.tm_min, dt.tm_sec, ms);
}

inline void TimestampMsToTm(uint64_t timestamp_ms, struct tm &tm, uint32_t &ms) {
	static const long kBiasYear = 1900;
	static const long kBiasDays = 25567;
	static const long lmos[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };
	static const long mos[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	uint64_t timestamp = timestamp_ms / 1000;
	ms = (uint32_t)(timestamp_ms % 1000);
	uint64_t secs = timestamp;
	int64_t days = kBiasDays;
	days += secs / kSecondsInDay;
	secs = secs % kSecondsInDay;
	tm.tm_hour = (int)(secs / kSecondsInHour);
	secs %= kSecondsInHour;
	tm.tm_min = (int)(secs / kSecondsInMinute);
	tm.tm_sec = (int)(secs % kSecondsInMinute);

	long i, year, mon;
	for (year = (long)(days / kDaysInYear);
		 days < (i = (((year - 1) / 4) + ((((year) & 03) || ((year) == 0)) ? mos[0] : lmos[0]) + kDaysInYear * year));)
		--year;
	days -= i;
	tm.tm_year = (int)(year + kBiasYear);
	mon = kMonthsInYear;
	if (((year) & 03) || ((year) == 0)) {
		while (days < mos[--mon]) {}
		tm.tm_mon = (int)mon;
		tm.tm_mday = (int)(days - mos[mon] + 1);
	} else {
		while (days < lmos[--mon]) {}
		tm.tm_mon = (int)mon;
		tm.tm_mday = (int)(days - lmos[mon] + 1);
	}
	tm.tm_wday = (int)TimestampMsToWeekDay((int64_t)timestamp_ms);
	tm.tm_isdst = 0;
}

inline int64_t Iso8601ToTimestampMs(const std::string &str_datetime) {
	struct tm t;
	std::memset(&t, 0, sizeof(t));
	const size_t str_size = str_datetime.size();
	uint32_t result_ms = 0;
	if (str_size < 10)
		return 0;

	t.tm_year = std::stoi(str_datetime.substr(0, 4));
	t.tm_mon = 0;
	t.tm_mday = 1;
	if (str_size >= 7 && str_datetime[4] == '-')
		t.tm_mon = std::stoi(str_datetime.substr(5, 2)) - 1;
	if (str_size >= 10 && str_datetime[7] == '-')
		t.tm_mday = std::stoi(str_datetime.substr(8, 2));
	if (str_size >= 13)
		t.tm_hour = std::stoi(str_datetime.substr(11, 2));
	if (str_size >= 16)
		t.tm_min = std::stoi(str_datetime.substr(14, 2));
	if (str_size >= 19)
		t.tm_sec = std::stoi(str_datetime.substr(17, 2));

	size_t timezone_start_pos = str_datetime.find('+', 19);
	if (timezone_start_pos == std::string::npos)
		timezone_start_pos = str_datetime.find('-', 19);

	std::string timezone;
	if (timezone_start_pos != std::string::npos)
		timezone = str_datetime.substr(timezone_start_pos);

	std::string millisec;
	size_t millisec_start_pos = str_datetime.find('.', 19);
	if (millisec_start_pos != std::string::npos) {
		size_t millisec_end_pos = str_datetime.find_first_of("+-Z", millisec_start_pos + 1);
		millisec = str_datetime.substr(millisec_start_pos,
			millisec_end_pos == std::string::npos ? std::string::npos : millisec_end_pos - millisec_start_pos);
	}

	int64_t timestamp = (int64_t)TmToTimestampMs(t, 0);
	if (timezone.size() > 5) {
		int gh = std::stoi(timezone.substr(1, 2));
		int gm = std::stoi(timezone.substr(4, 2));
		int offset = (gh * 3600 + gm * 60) * 1000;
		if (timezone[0] == '+') timestamp -= offset;
		else if (timezone[0] == '-') timestamp += offset;
	} else if (timezone.size() > 3) {
		int gh = std::stoi(timezone.substr(1, 2));
		int offset = (gh * 3600) * 1000;
		if (timezone[0] == '+') timestamp -= offset;
		else if (timezone[0] == '-') timestamp += offset;
	}

	if (!millisec.empty())
		result_ms = (uint32_t)std::stoi(millisec.substr(1));
	return timestamp + (int64_t)result_ms;
}

inline uint32_t TimestampMsToDayHour(int64_t timestamp_ms) {
	return (uint32_t)((timestamp_ms / (kSecondsInHour * 1000LL)) % kHoursInDay);
}

inline uint32_t TimestampMsToHourMinute(int64_t timestamp_ms) {
	return (uint32_t)((timestamp_ms / (kSecondsInMinute * 1000LL)) % kMinutesInHour);
}

inline std::string TimestampMsToIso8601(int64_t timestamp_ms, int64_t timezone_offset_ms) {
	struct tm t;
	uint32_t ms = 0;
	TimestampMsToTm((uint64_t)(timestamp_ms + timezone_offset_ms), t, ms);

	char zone[16] = {};
	if (timezone_offset_ms == 0)
		std::snprintf(zone, sizeof(zone), "Z");
	else {
		int64_t temp = timezone_offset_ms < 0 ? -timezone_offset_ms : timezone_offset_ms;
		std::snprintf(zone, sizeof(zone), "%c%02u:%02u",
			timezone_offset_ms > 0 ? '+' : '-',
			TimestampMsToDayHour(temp),
			TimestampMsToHourMinute(temp));
	}

	char body[40] = {};
	std::snprintf(body, sizeof(body), "%04d-%02d-%02dT%02d:%02d:%02d.%03u",
		t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, ms);
	return std::string(body) + zone;
}

} // namespace date
} // namespace tinyjs

#endif
