#include "memorymanager.h"
#include <sys/time.h>
#include <limits.h>
#include <stdlib.h>
#define max(a,b) (((a)>(b))?(a):(b))
MemoryManager::MemoryManager(int numPages)
{
	bitMap = new BitMap(numPages);
	lock = new Lock("lock of memory manager");
	processMap = (int *)malloc(sizeof(int)*100);
	entries=NULL;
}

MemoryManager::~MemoryManager()
{
	delete bitMap;
	delete lock;
}

int
MemoryManager::AllocPage()
{
	lock->Acquire();
	int ret = bitMap->Find();
	lock->Release();
	return ret;
}


int
MemoryManager::AllocByForce()
{	
	lock->Acquire();
	//int ret = rand()%NumPhysPages;	//for random replacement
	int ret=0;


	int a[NumPhysPages];
	for(int i=0;i<NumPhysPages;i++)a[i]=LONG_MIN;
	for(int i=1;i<machine->pageTableSize;i++){	
		if(machine->pageTable[i].physicalPage!=-1 && machine->pageTable[i].dirty==false)
		{
			a[machine->pageTable[i].physicalPage]=max(machine->pageTable[i].time,a[machine->pageTable[i].physicalPage]);
			//if(machine->pageTable[i].time<min){min=machine->pageTable[i].time;ret=machine->pageTable[i].physicalPage;}
		}
	}
	int min=a[0];
	for(int i=0;i<NumPhysPages;i++){	
		printf("%d %d\n",i,a[i]);
	}
	for(int i=1;i<NumPhysPages;i++){	
		if(a[i]<min){min=a[i];ret=i;}
	}
	lock->Release();
	return ret;
}

int
MemoryManager::AllocPage(int processNo,TranslationEntry &entries)
{
	lock->Acquire();
	int ret = bitMap->Find();
	if(ret!=-1){
		processMap[nowSize++]=processNo;
		entries=entries;
	}
	lock->Release();
	return ret;
}

void
MemoryManager::FreePage(int physPageNum)
{
	lock->Acquire();
	bitMap->Clear(physPageNum);
	lock->Release();
}

bool
MemoryManager::PageIsAllocated(int physPageNum)
{
	lock->Acquire();
	bool ret = bitMap->Test(physPageNum);
	lock->Release();
	return ret;
}

bool
MemoryManager::IsAnyPageFree()
{
	lock->Acquire();
	bool ret;
	if(bitMap->NumClear() == 0)
		ret = false;
	else
		ret = true;
	lock->Release();
	return ret;
}

int
MemoryManager::NumFreePages()
{
	lock->Acquire();
	int ret = bitMap->NumClear();
	lock->Release();
	return ret;
}
