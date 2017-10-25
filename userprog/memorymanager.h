#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H
#include "machine.h"
#include "copyright.h"
#include "system.h"
#ifndef BITMAP_H
#include "bitmap.h"
#endif

#ifndef SYNCH_H
#include "synch.h"
#endif

class MemoryManager{
public:
	MemoryManager(int numPages);
	~MemoryManager();
	int AllocPage();
	int AllocPage(int processNo,TranslationEntry &entries);
	int AllocByForce();
	void FreePage(int physPageNum);
	bool PageIsAllocated(int physPageNum);
	bool IsAnyPageFree();
	int NumFreePages();
private:
	BitMap *bitMap;
	Lock *lock;
	int *processMap;
	TranslationEntry *entries;
	int nowSize=0;

};

#endif
