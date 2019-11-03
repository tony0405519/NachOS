// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//--------------------------------------------------------
// 自定義函數，印出Thread的名稱及burstTime
//--------------------------------------------------------
void PrintThreadBurstTime(Thread *t) {
    t->Print();
    printf(": %d    ",t->getBurstTime());
}

//----------------------------------------------------------------------
// Compare function
//----------------------------------------------------------------------
int SJFCompare(Thread *a, Thread *b) {
    if(a->getBurstTime() == b->getBurstTime())
        return 0;
    return a->getBurstTime() > b->getBurstTime() ? 1 : -1;
}
int PriorityCompare(Thread *a, Thread *b) {
    if(a->getPriority() == b->getPriority())
        return 0;
    return a->getPriority() > b->getPriority() ? 1 : -1;
}
int FIFOCompare(Thread *a, Thread *b) {
    return 1;
}
//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{
	Scheduler(RR);
    DEBUG(dbgScheduler,"Schduler type: " << "RR");
}

Scheduler::Scheduler(SchedulerType type)
{
	schedulerType = type;
    const char* schType;
	switch(schedulerType) {
        case FIFO:
		    readyList = new SortedList<Thread *>(FIFOCompare);
            schType = "FIFO";
		    break;
        case SJF:
		    readyList = new SortedList<Thread *>(SJFCompare);
            schType = "SJF";
        	break;
        case SRTF:
            readyList = new SortedList<Thread *>(SJFCompare);
            schType = "SRTF";
            break;
    	case RR:
        	readyList = new List<Thread *>;
            schType = "RR";
        	break;
    	case Priority:
		    readyList = new SortedList<Thread *>(PriorityCompare);
            schType = "Priority";
        	break;
   	}
    DEBUG(dbgScheduler,"Schduler type: " << schType);
	toBeDestroyed = NULL;

    // for sleep list
    sleepingList = std::vector<Sleeper>();
    cpuInterrupt = 0;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    
    thread->setStatus(READY);
    readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
	return NULL;
    } else {
        // 自定義debug flag，印出所有的thread的bursttime
        if (debug->IsEnabled(dbgScheduler)) {
            readyList->Apply(PrintThreadBurstTime);
            printf("\n");
        }
    	return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
 
//	cout << "Current Thread" <<oldThread->getName() << "    Next Thread"<<nextThread->getName()<<endl;
   
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
#ifdef USER_PROGRAM
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}

//----------------------------------------------------------------------
// Scheduler::anyThreadWoken
//      
//      判斷在 自定義 休眠型 wait queue中的每一個thread，是否有已經經結束休眠者
//      
//      若有，則將該thread放回ready queue，並且回傳True
//
//----------------------------------------------------------------------
bool Scheduler::anyThreadWoken()
{
    bool woken = false;
    cpuInterrupt++;
    if (this->sleepingListEmpty()) return woken;
    for(std::vector<Sleeper>::iterator it = sleepingList.begin(); it != sleepingList.end(); ) {
        if(it->due <= this->cpuInterrupt) {
            woken = true;
            // 儲存起床的thread 指標
            Thread * wokenThread = it->sleepTh;
            cout << "sleepList::PutToReady Thread woken: " << wokenThread->getName() << endl;
            // 將sleeper從 sleeping list remove，並且更新iter
            it = sleepingList.erase(it);
            // 將起床的thread送到readylist
            this->ReadyToRun(wokenThread);
        } 
        else it++;
    }
    return woken;
}

//----------------------------------------------------------------------
// Scheduler::PutToSleep
//      
//      將要放入休眠的thread記錄到 自定義 休眠型 wait queue
//      
//      並且讓出CPU
//
//      "thread" -- 為待休眠thread；"after" -- 為休眠時間(以Alarm::CallBack()觸發中斷時為計數單位)
//----------------------------------------------------------------------
void Scheduler::PutToSleep(Thread * thread, int after)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);// 檢查是否interrupt已經disabled
    sleepingList.push_back(Sleeper(thread, cpuInterrupt + after));
    thread->Sleep(FALSE);
}
