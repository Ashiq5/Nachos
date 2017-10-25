// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#include <sys/time.h>
#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include "synch.h"
#include "memorymanager.h"
#include <limits.h>
#include <map>
extern MemoryManager *memoryManager;
extern MemoryManager *swapMemoryManager;
extern Lock *memoryLock;
NoffHeader noffH;
//map <int,int> swapmap;
int swapmap[50000];
struct timeval tp;
int tim=0;
OpenFile *bsFile;
//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    exctbl=executable;
    
    unsigned int i, j, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    //ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
    	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
    	pageTable[i].physicalPage = -1;
	pageTable[i].time=LONG_MAX;
        /*if(memoryManager->IsAnyPageFree() == true)
            pageTable[i].physicalPage = memoryManager->AllocPage();
        else
        {
            for(j = 0; j < i; ++j)
                memoryManager->FreePage(pageTable[j].physicalPage);
            ASSERT(false);
        }*/
	pageTable[i].valid = false;
    	//pageTable[i].valid = true;
    	pageTable[i].use = false;
    	pageTable[i].dirty = false;
    	pageTable[i].readOnly = false;  // if the code segment was entirely on 
                    					// a separate page, we could set its 
                    					// pages to be read-only
    }
    
    // zero out the entire address space, to zero the unitialized data segment 
    // and the stack segment
    //bzero(machine->mainMemory, size);
    memoryLock->Acquire();
    for(i = 0; i < numPages; ++i)
    {
        //bzero(&machine->mainMemory[pageTable[i].physicalPage * PageSize], PageSize);
    }

    
    // then, copy in the code and data segments into memory
    unsigned int numPagesForCode = divRoundUp(noffH.code.size, PageSize);
    DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
		noffH.code.virtualAddr, noffH.code.size);
    for(i = 0; i < numPagesForCode; ++i)
    {
        /*executable->ReadAt(&(machine->mainMemory[ pageTable[i].physicalPage * PageSize ]),
                            PageSize, 
                            noffH.code.inFileAddr + i * PageSize);*/
    }

    unsigned int numPagesForData = divRoundUp(noffH.initData.size, PageSize);

    DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
        noffH.initData.virtualAddr, noffH.initData.size);
    for(j = numPagesForCode; j < numPagesForCode + numPagesForData; ++j)
    {
        /*executable->ReadAt(&(machine->mainMemory[ pageTable[i].physicalPage * PageSize ]),
                            PageSize, 
                            noffH.initData.inFileAddr + (j - numPagesForCode) * PageSize);*/
    }
    memoryLock->Release();

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
   delete exctbl;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

int AddrSpace::LoadIntoFreePage(int addr,int phyPageNo) 
{
	int vpn= addr/PageSize;int rem;
	pageTable[vpn].virtualPage = vpn;	
    	pageTable[vpn].physicalPage = phyPageNo;
	pageTable[vpn].valid = true;
	gettimeofday(&tp, NULL);
	//pageTable[vpn].time=tp.tv_sec * 1000 + tp.tv_usec / 1000;
	pageTable[vpn].time=tim++;
	printf("In addrspace %d %d\n",phyPageNo,pageTable[vpn].time);
    	pageTable[vpn].use = false;
    	pageTable[vpn].dirty = false;
    	pageTable[vpn].readOnly = false;
	int pageStartAddr = vpn*PageSize;
        int pageEndAddr = pageStartAddr + PageSize - 1;
        int codeEndAddr = noffH.code.virtualAddr+noffH.code.size-1;
        int initDataEndAddr = noffH.initData.virtualAddr+noffH.initData.size-1;
	memoryLock->Acquire();
	if(isSwapPageExists(vpn)==true){
		printf("df");
		loadFromSwapSpace(vpn);
		memoryLock->Release();
		return 0;
	}
	if(addr>=noffH.code.virtualAddr && addr<noffH.code.virtualAddr+noffH.code.size){//code segment
		
    		int codeoffset=(addr-noffH.code.virtualAddr)/PageSize;
    		int codeSize;
		    
		if(noffH.code.size-codeoffset*PageSize<PageSize)codeSize=noffH.code.size-codeoffset*PageSize;
		else codeSize=PageSize;
		
    		//int codeSize=min(noffH.code.size-codeoffset*PageSize, PageSize);

		exctbl->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize ]),
                            codeSize, 
                            noffH.code.inFileAddr + addr - noffH.code.virtualAddr);

		if(codeSize<PageSize){
			rem=PageSize-codeSize;
			//2 cases
			//remaining part in initdata
			if(noffH.initData.size>0 && pageEndAddr<=initDataEndAddr){
				exctbl->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize +codeSize]),
                        	rem, 
                            	noffH.initData.inFileAddr);
			}
			//remaining part in uninitdata or stack
			else{
				bzero(&machine->mainMemory[ pageTable[vpn].physicalPage* PageSize+codeSize], rem);
			}
		}

	}
	
	else if(addr>=noffH.initData.virtualAddr && addr<noffH.initData.virtualAddr+noffH.initData.size){//init data segment
		
    		int initDataoffset=(addr-noffH.initData.virtualAddr)/PageSize;
    		int initDataSize;
		    
		if(noffH.initData.size-initDataoffset*PageSize<PageSize)initDataSize=noffH.initData.size-initDataoffset*PageSize;
		else initDataSize=PageSize;
    		
		exctbl->ReadAt(&(machine->mainMemory[ pageTable[vpn].physicalPage * PageSize ]),
                            initDataSize, 
                            noffH.initData.inFileAddr + addr - noffH.initData.virtualAddr);

		if(initDataSize<PageSize){
			rem=PageSize-initDataSize;
			//remaining part in uninitdata or stack
			
			bzero(&machine->mainMemory[ pageTable[vpn].physicalPage* PageSize+initDataSize], rem);
		}

	}
	
	else if(addr>=noffH.uninitData.virtualAddr && addr<noffH.uninitData.virtualAddr+noffH.uninitData.size){
		
		bzero(&( machine->mainMemory[pageTable[vpn].physicalPage * PageSize] ), PageSize);

	}

	else{
		bzero(&( machine->mainMemory[pageTable[vpn].physicalPage * PageSize] ), PageSize);

	}
	memoryLock->Release();
	return 0;
	

}
void AddrSpace::init()
{
	
	//bsFile=bsFile;
	char *bsFileName;
	bsFileName = new char[10];
	sprintf(bsFileName, "SwapFile");
	fileSystem->Create(bsFileName, 500 * PageSize);
	bsFile = fileSystem->Open(bsFileName);
	for(int i=0;i<machine->pageTableSize;i++)swapmap[i]=-1;
}
void AddrSpace::saveIntoSwapSpace(int vpn) 
{
		memoryLock->Acquire();
		int pageNo=swapMemoryManager->AllocPage();
		swapmap[vpn]=pageNo;
		int offset = pageTable[vpn].virtualPage * PageSize;
		int physAddr = pageTable[vpn].physicalPage * PageSize;
		
		bsFile->WriteAt(&machine->mainMemory[physAddr], PageSize, offset);
		memoryLock->Release();
		 
}

void AddrSpace::loadFromSwapSpace(int vpn) 
{
	memoryLock->Acquire();
    	if (pageTable[vpn].valid==true){
		int offset = pageTable[vpn].virtualPage * PageSize;
		int physAddr = pageTable[vpn].physicalPage * PageSize;
		
		bsFile->ReadAt(&machine->mainMemory[physAddr], PageSize, offset);
		
	}
	memoryLock->Release();
}

bool AddrSpace::isSwapPageExists(int vpn) 
{
	//if(swapmap[vpn]!=-1)return true;
    return false;
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
