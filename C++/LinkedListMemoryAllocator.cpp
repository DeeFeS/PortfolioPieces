#include "LinkedListMemoryAllocator.h"
#include <iostream>
#include <string>
#include <cassert>

LinkedListMemoryAllocator::LinkedListMemoryAllocator(size_t nbBytes)
	: m_BlockAmount{ size_t((nbBytes + 2 * sizeof(SingleLinkBlock) - 1) / sizeof(SingleLinkBlock)) }
{
	//std::cout << "Single | Blocks: " << m_BlockAmount << " Bytes: " << nbBytes << std::endl;
	m_pHead = reinterpret_cast<SingleLinkBlock*>(malloc(m_BlockAmount * SingleLinkBlock::size));
	m_pHead->count = 0;
	m_pHead->pNext = m_pHead + 1;
	m_pHead->pNext->pNext = nullptr;
	m_pHead->pNext->count = m_BlockAmount - 1;
}

LinkedListMemoryAllocator::~LinkedListMemoryAllocator()
{
	free(reinterpret_cast<void*>(m_pHead));
}

void* LinkedListMemoryAllocator::Acquire(size_t nbBytes)
{
	size_t blockAmount = CalculateBlockAmount(nbBytes);
	bool notEnoughSpaceLeft = true;
	auto pPrevious = m_pHead;
	auto pNext = m_pHead->pNext;
	while (pNext != nullptr)
	{
		if (pNext->count >= blockAmount)
		{
			notEnoughSpaceLeft = false;
			break;
		}
		pPrevious = pNext;
		pNext = pNext->pNext;
	}

	if (notEnoughSpaceLeft)
	{
		throw std::exception("out of memory");
		//return nullptr;
	}

	if (pNext->count > blockAmount)
	{
		// if free block > needed, setup new header

		SingleLinkBlock* pNextFree = pNext + blockAmount;
		pNextFree->count = pNext->count - blockAmount;
		pNextFree->pNext = pNext->pNext;
		pPrevious->pNext = pNextFree;
	}
	else
	{
		// if free block == needed, simply copy next pointer
		pPrevious->pNext = pNext->pNext;
	}
	pNext->count = blockAmount;
	return pNext->data;
}

void LinkedListMemoryAllocator::Release(void* pStart)
{
	// Check if pStart is part of buffer
	if (pStart == nullptr || pStart < m_pHead + 1 || m_pHead + m_BlockAmount <= pStart)
	{
		return;
	}
	SingleLinkBlock* pBlock = reinterpret_cast<SingleLinkBlock*>(reinterpret_cast<SingleLinkHeader*>(pStart) - 1);
	auto pPrevious = m_pHead;
	auto pNext = m_pHead->pNext;
	while (pNext != nullptr && pNext < pBlock)
	{
		pPrevious = pNext;
		pNext = pNext->pNext;
	}

	// pBlock is after last empty
	if (pNext == nullptr)
	{
		if (pPrevious + pPrevious->count == pBlock)
		{
			pPrevious->count += pBlock->count;
		}
		else // another block between last free and block
		{
			pPrevious->pNext = pBlock;
			pBlock->pNext = nullptr;
		}
		return;
	}

	// block behind is empty
	if (pBlock + pBlock->count == pNext)
	{
		pBlock->count += pNext->count;
		pBlock->pNext = pNext->pNext;
	}
	else // next free block is somewhere else
	{
		pBlock->pNext = pNext;
	}

	if (pPrevious + pPrevious->count == pBlock)
	{
		pPrevious->count += pBlock->count;
		pPrevious->pNext = pBlock->pNext;
	}
	else
	{
		pPrevious->pNext = pBlock;
	}
}

std::string LinkedListMemoryAllocator::UsageToString(const char header, const  char begin, const  char unused, const  char used) const
{
	if (!m_pHead)
	{
		return "LLMA | Head is nullptr";
	}
	std::string string{ header };
	SingleLinkBlock* pCurrent = m_pHead->pNext;

	for (size_t i = 1; i < m_BlockAmount; i++)
	{
		if (pCurrent && m_pHead + i == pCurrent)
		{
			string += begin;
			string += std::string(pCurrent->count - 1, unused);
			i += pCurrent->count - 1;
			pCurrent = pCurrent->pNext;
		}
		else
		{
			string += used;
		}
	}
	return string;
}

void LinkedListMemoryAllocator::Visualize() const
{
	std::cout << UsageToString() << std::endl;
}

bool LinkedListMemoryAllocator::CheckMemory(std::string expected, const char header, const char begin, const char unused, const char used) const
{
	std::string result{ UsageToString(header, begin, unused, used) };
	return expected == result;
}
