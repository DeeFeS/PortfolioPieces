#pragma once
#include "MemoryAllocator.h"
#include <string>
class DoubleLinkedListMemoryAllocator : public MemoryAllocator
{
public:
	DoubleLinkedListMemoryAllocator(size_t blockAmount);
	virtual ~DoubleLinkedListMemoryAllocator();
	virtual void* Acquire(size_t nbBytes = 0) override;
	virtual void Release(void* pStart) override;
	DoubleLinkBlock* GetHead() const { return m_pHead; };
	size_t GetBlockAmount() const { return m_BlockAmount; };
	std::string UsageToString(const char header = CHeader, const char begin = CBegin, const char unused = CUnused, const char used = CUsed) const;
	void Visualize() const;
	void ListAdresses() const;
	inline static size_t CalculateBlockAmount(const size_t nbBytes)
	{
		return size_t((nbBytes + sizeof(DoubleLinkHeader) + sizeof(DoubleLinkBlock) - 1) / sizeof(DoubleLinkBlock));
	};
private:
	DoubleLinkBlock* m_pHead;
	size_t m_BlockAmount;
#ifdef _DEBUG
	size_t m_UsedBlocks;
#endif

	void Unlink(DoubleLinkBlock* pBlock);
	void InsertAfter(DoubleLinkBlock* pInsert, DoubleLinkBlock* pPrevious);
};

