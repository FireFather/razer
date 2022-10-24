#pragma once
#include <intrin.h>

inline int lsb(const unsigned long long b)
{
	unsigned long idx;
	_BitScanForward64(&idx, b);
	return static_cast<int>(idx);
}

inline int msb(const unsigned long long b)
{
	unsigned long idx;
	_BitScanReverse64(&idx, b);
	return static_cast<int>(idx);
}

inline int pop_lsb(unsigned long long* b)
{
	const auto s = lsb(*b);
	*b &= *b - 1;
	return s;
}

inline int popcnt(const unsigned long long b)
{
	return static_cast<int>(_mm_popcnt_u64(b));
}

inline bool more_than_one(const unsigned long long b)
{
	return b & b - 1;
}
