// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to 
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
    timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice 
//      if we're currently running something (in other words, not idle).
//	Also, to keep from looping forever, we check if there's
//	nothing on the ready list, and there are no other pending
//	interrupts.  In this case, we can safely halt.
//----------------------------------------------------------------------

void Alarm::CallBack() {// 週期性的打斷CPU
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();
    bool woken = _sleepList.PutToReady(); // 自定義計數器++；檢查是否有thread已經休眠結束，可以放回Ready Queue
    kernel->currentThread->setPriority(kernel->currentThread->getPriority() - 1);
    if (status == IdleMode && !woken && _sleepList.IsEmpty()) {// is it time to quit?
        if (!interrupt->AnyFutureInterrupts()) {// 有任何在排隊的interrupts嗎？
            timer->Disable();   // turn off the timer
        }
    } else {                    // there's someone to preempt
	if(kernel->scheduler->getSchedulerType() == RR ||
            kernel->scheduler->getSchedulerType() == Priority ) {
		interrupt->YieldOnReturn();
	}
    }
}

//----------------------------------------------------------------------
// Alarm::WaitUntil
//      這是user program呼叫Sleep(int)後，經過exception.cc的進入點
//
//      會把當前正在執行的thread記錄下來並送入休眠狀態
//
//----------------------------------------------------------------------
void Alarm::WaitUntil(int x) {
    //關中斷，不讓其他thread能夠強制進入(preempt)，剔除本thread
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    //將目前要執行的thread放入休眠
    Thread* t = kernel->currentThread;
    cout << "Alarm::WaitUntil go sleep" << endl;
    _sleepList.PutToSleep(t, x);
    //開中斷
    kernel->interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// sleepList::IsEmpty
//      
//      判斷是否 自定義 休眠型 wait queue 是否已經空了
//----------------------------------------------------------------------
bool sleepList::IsEmpty() {
    return _threadlist.size() == 0;
}

//----------------------------------------------------------------------
// sleepList::PutToSleep
//      
//      將要放入休眠的thread記錄到 自定義 休眠型 wait queue
//      
//      並且讓出CPU
//
//      "t" -- 為待休眠thread；"x" -- 為休眠時間(以Alarm::CallBack()觸發中斷時計數單位)
//----------------------------------------------------------------------
void sleepList::PutToSleep(Thread*t, int x) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);// 檢查是否interrupt已經disabled
    _threadlist.push_back(sleepThread(t, _current_interrupt + x));// 將該thread放入列表中(自定義Waiting Queue)
    t->Sleep(false);// 送入休眠(unfinished，該thread不會被destroyed)，然後讓出CPU
}

//----------------------------------------------------------------------
// sleepList::PutToReady
//      
//      判斷在 自定義 休眠型 wait queue中的每一個thread，是否有已經經結束休眠者
//      
//      若有，則將該thread放回ready queue，並且回傳True
//
//----------------------------------------------------------------------
bool sleepList::PutToReady() {
    bool woken = false; // 當有thread休眠結束，就為true
    _current_interrupt ++; // 自定義計數器計數
    for(std::list<sleepThread>::iterator it = _threadlist.begin();
        it != _threadlist.end(); ) {
        if(_current_interrupt >= it->when) {
            woken = true;
            cout << "sleepList::PutToReady Thread woken" << endl;
            kernel->scheduler->ReadyToRun(it->sleeper); // 放入系統ready queue
            it = _threadlist.erase(it);
        } else {
            it++;
        }
    }
    return woken;
}




