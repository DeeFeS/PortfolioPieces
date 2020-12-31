#include "DoubleLinkedListMemoryAllocator.h"
#include <iostream>

DoubleLinkedListMemoryAllocator::DoubleLinkedListMemoryAllocator(size_t nbBytes)
	: m_BlockAmount{ size_t((nbBytes + 2 * sizeof(DoubleLinkBlock) - 1) / sizeof(DoubleLinkBlock)) }
#ifdef _DEBUG
	, m_UsedBlocks{ 0 }
#endif
{
#ifdef _DEBUG
	m_pHead = reinterpret_cast<DoubleLinkBlock*>(calloc(m_BlockAmount, sizeof(DoubleLinkBlock)));
#else
	m_pHead = reinterpret_cast<DoubleLinkBlock*>(malloc(m_BlockAmount * sizeof(DoubleLinkBlock)));
#endif

	if (!m_pHead)
		throw std::exception("out of memory");

	m_pHead->count = 0;
	m_pHead->status = Status::reserved;
	m_pHead->links.pNext = m_pHead + 1;
	m_pHead->links.pPrevious = m_pHead + 1;

	m_pHead->links.pNext->status = Status::free;
	m_pHead->links.pNext->links.pNext = m_pHead;
	m_pHead->links.pNext->links.pPrevious = m_pHead;
	m_pHead->links.pNext->count = m_BlockAmount - 1;
}

DoubleLinkedListMemoryAllocator::~DoubleLinkedListMemoryAllocator()
{
	free(reinterpret_cast<void*>(m_pHead));
}

void* DoubleLinkedListMemoryAllocator::Acquire(size_t nbBytes)
{
	size_t blockAmount = size_t((nbBytes + sizeof(DoubleLinkHeader) + sizeof(DoubleLinkBlock) - 1) / sizeof(DoubleLinkBlock));
	DoubleLinkBlock* pCurrent = m_pHead->links.pNext;
	const DoubleLinkBlock* pEnd = m_pHead + m_BlockAmount;
	while (pCurrent != m_pHead)
	{
		DoubleLinkBlock* pNext = pCurrent + pCurrent->count;
		while (pNext < pEnd && pNext->status == Status::free)
		{
			pCurrent->count += pNext->count;
			Unlink(pNext);
			pNext = pCurrent + pCurrent->count;
		}
		if (pCurrent->count >= blockAmount)
			break;

		pCurrent = pCurrent->links.pNext;
	}

	if (pCurrent->count < blockAmount)
		throw std::exception("out of memory");

	// found big enough space
	pCurrent->status = Status::reserved;
	if (pCurrent->count > blockAmount)
	{
		DoubleLinkBlock* pNew = pCurrent + blockAmount;
		pNew->status = Status::free;
		pNew->count = pCurrent->count - blockAmount;
		pCurrent->count = blockAmount;
		InsertAfter(pNew, m_pHead);
	}
	Unlink(pCurrent);

#ifdef _DEBUG
	m_UsedBlocks += blockAmount;
#endif
	return pCurrent->data;
}

void DoubleLinkedListMemoryAllocator::Release(void* pStart)
{
	// Check if pStart is part of buffer
	if (pStart == nullptr || pStart < m_pHead + 1 || reinterpret_cast<DoubleLinkHeader*>(pStart) - 1 > m_pHead + m_BlockAmount)
		return;

	DoubleLinkBlock* pBlock = reinterpret_cast<DoubleLinkBlock*>(reinterpret_cast<char*>(pStart) - sizeof(DoubleLinkHeader));
	InsertAfter(pBlock, m_pHead);
	pBlock->status = Status::free;

#ifdef _DEBUG
	m_UsedBlocks -= pBlock->count;
#endif
}

std::string DoubleLinkedListMemoryAllocator::UsageToString(const char header, const  char begin, const  char unused, const  char used) const
{
	if (!m_pHead)
		return "DLLMA | Head is nullptr";

	std::string string{ header };
	DoubleLinkBlock* pCurrent = m_pHead + 1;

	DoubleLinkBlock* pEnd = m_pHead + m_BlockAmount;

	while (pCurrent < pEnd)
	{
		if (pCurrent->status == Status::free)
		{
			if (pCurrent == m_pHead->links.pNext)
				string += "b";
			else if (pCurrent == m_pHead->links.pPrevious)
				string += "e";
			else
				string += begin;
			string += std::string(pCurrent->count - 1, unused);
		}
		else
			string += std::string(pCurrent->count, used);
		pCurrent = pCurrent + pCurrent->count;
	}
	return string;
}

void DoubleLinkedListMemoryAllocator::Visualize() const
{
	std::cout << UsageToString() << std::endl;
}

void DoubleLinkedListMemoryAllocator::ListAdresses() const
{
	DoubleLinkBlock* pCurrent = m_pHead;
	std::cout << "H: " << pCurrent->links.pPrevious << " < " << pCurrent << " > " << pCurrent->links.pNext << std::endl;

	pCurrent = pCurrent->links.pNext;
	size_t count = 1;
	while (pCurrent != m_pHead)
	{
		std::cout << count << ": " << pCurrent->links.pPrevious << " < " << pCurrent << " > " << pCurrent->links.pNext << std::endl;
		pCurrent = pCurrent->links.pNext;
		count++;
	}
	std::cout << std::endl;
}

void DoubleLinkedListMemoryAllocator::Unlink(DoubleLinkBlock* pBlock)
{
	pBlock->links.pNext->links.pPrevious = pBlock->links.pPrevious;
	pBlock->links.pPrevious->links.pNext = pBlock->links.pNext;
}

void DoubleLinkedListMemoryAllocator::InsertAfter(DoubleLinkBlock* pInsert, DoubleLinkBlock* pPrevious)
{
	pPrevious->links.pNext->links.pPrevious = pInsert;
	pInsert->links.pNext = pPrevious->links.pNext;
	pInsert->links.pPrevious = pPrevious;
	pPrevious->links.pNext = pInsert;
}
