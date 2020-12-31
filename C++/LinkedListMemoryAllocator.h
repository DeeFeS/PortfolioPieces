#pragma once
#include "MemoryAllocator.h"
#include "MemoryBlock.h"
#include <string>

class LinkedListMemoryAllocator : public MemoryAllocator
{
public:
	LinkedListMemoryAllocator(size_t nbBytes);
	virtual ~LinkedListMemoryAllocator();
	virtual void* Acquire(size_t nbBytes = 0) override;
	virtual void Release(void* pStart) override;
	SingleLinkBlock* GetHead() const { return m_pHead; };
	size_t GetBlockAmount() const { return m_BlockAmount; };
	std::string UsageToString(const char header = CHeader, const char begin = CBegin, const char unused = CUnused, const char used = CUsed) const;
	void Visualize() const;
	bool CheckMemory(std::string expected, const char header = CHeader, const char begin = CBegin, const char unused = CUnused, const char used = CUsed) const;
	inline static size_t CalculateBlockAmount(const size_t nbBytes)
	{
		return size_t((nbBytes + sizeof(SingleLinkHeader) + sizeof(SingleLinkBlock) - 1) / sizeof(SingleLinkBlock));
	};
private:
	SingleLinkBlock* m_pHead;
	size_t m_BlockAmount;
};