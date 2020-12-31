#pragma once
#include <cstdlib>
#include <limits>

struct SingleLinkHeader
{
	size_t count;
};

struct SingleLinkBlock : public SingleLinkHeader
{
	const static int size = 16;
	union
	{
		SingleLinkBlock* pNext;
		char data[size - sizeof(SingleLinkHeader)];
	};
};

enum class Status : unsigned { free, reserved };

struct DoubleLinkHeader
{
	Status status : 1;
	size_t count : std::numeric_limits<size_t>::digits - 1;
};

struct DoubleLinkBlock : public DoubleLinkHeader
{
#if not defined (_WIN64)
	enum { size = 16 };
#endif
#if defined (_WIN64)
	enum { size = 32 };
#endif
	struct Links
	{
		DoubleLinkBlock* pNext;
		DoubleLinkBlock* pPrevious;
	};
	union
	{
		Links links;
		char data[size - sizeof(DoubleLinkHeader)];
	};
};