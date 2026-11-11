// backpack_sidecar_policy.cpp - The backpack-first sidecar's text, pure. Any thread; no lock.
#include "inventory/backpack_sidecar_policy.h"
#include "inventory/backpack_policy.h"
#include "inventory/operator_policy.h"
#include <stdio.h>
#include <string.h>

namespace keo_inventory {

std::string SidecarFormat(const SidecarEntry* entries, int n, int* formatFailed)
{
	std::string text(SIDECAR_HEADER);
	text += '\n';
	for (int i = 0; entries && i < n; ++i)
	{
		char key[64];
		int len = HandKeyFormat(entries[i].key, key, (int)sizeof key);
		if (len <= 0)
		{
			if (formatFailed)
				++*formatFailed;
			continue;
		}
		if (entries[i].on >= 0)
		{
			text.append(key, (size_t)len);
			text += entries[i].on ? " 1\n" : " 0\n";
		}
		if (OperatorFillPercentValid(entries[i].operatorFillPercent))
		{
			char value[8];
			sprintf_s(value, " %d\n", entries[i].operatorFillPercent);
			text += "# fill ";
			text.append(key, (size_t)len);
			text += value;
		}
	}
	return text;
}

// A line holding nothing but spaces and tabs.
static bool LineBlank(const char* line, int len)
{
	for (int i = 0; i < len; ++i)
		if (line[i] != ' ' && line[i] != '\t')
			return false;
	return true;
}

// One data line into out: a bad line is counted and skipped, a key already in out takes this
// line's value in its first line's place, and a new key past max is counted, not written.
static void ParseEntryLine(const char* line, int len, bool fill, SidecarEntry* out, int max, SidecarParseResult* r)
{
	unsigned f[7];
	if (ParseUnsignedFields(line, len, f, 7) != 6
	    || (fill ? !OperatorFillPercentValid(f[5]) : f[5] > 1))
	{
		++r->badLines;
		return;
	}
	game::HandKey k = { f[0], f[1], f[2], f[3], f[4] };
	if (game::HandKeyIsNull(k))
	{
		++r->badLines;
		return;
	}
	for (int j = 0; j < r->accepted; ++j)
	{
		if (game::HandKeyEqual(out[j].key, k))
		{
			int& field = fill ? out[j].operatorFillPercent : out[j].on;
			if (fill ? field != 0 : field >= 0)
				++r->duplicates;
			field = (int)f[5];
			return;
		}
	}
	if (!out || r->accepted >= max)
	{
		++r->overflow;
		return;
	}
	out[r->accepted].key = k;
	out[r->accepted].on = fill ? -1 : (int)f[5];
	out[r->accepted].operatorFillPercent = fill ? (int)f[5] : 0;
	++r->accepted;
}

SidecarParseResult SidecarParse(const char* text, int len, SidecarEntry* out, int max)
{
	SidecarParseResult r;
	memset(&r, 0, sizeof r);
	if (!text || len <= 0)
		return r;
	if (len > SIDECAR_MAX_BYTES)
	{
		r.refused = true;
		return r;
	}

	const int headerLen = (int)strlen(SIDECAR_HEADER);
	bool sawHeader = false;
	int pos = 0;
	while (pos < len)
	{
		int end = pos;
		while (end < len && text[end] != '\n')
			++end;
		const char* line = text + pos;
		int lineLen = end - pos;
		pos = end + 1;
		if (lineLen > 0 && line[lineLen - 1] == '\r')
			--lineLen;
		if (LineBlank(line, lineLen))
			continue;
		if (line[0] == '#')
		{
			const char fillPrefix[] = "# fill ";
			const int prefixSize = sizeof(fillPrefix) - 1;
			if (sawHeader && lineLen >= prefixSize && !memcmp(line, fillPrefix, prefixSize))
				ParseEntryLine(line + prefixSize, lineLen - prefixSize, true, out, max, &r);
			continue;
		}
		if (!sawHeader)
		{
			if (lineLen != headerLen || memcmp(line, SIDECAR_HEADER, (size_t)headerLen) != 0)
				break;
			sawHeader = true;
			continue;
		}
		ParseEntryLine(line, lineLen, false, out, max, &r);
	}

	if (!sawHeader)
	{
		memset(&r, 0, sizeof r);
		r.refused = true;
	}
	return r;
}

} // namespace keo_inventory
