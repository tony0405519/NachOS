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

void Alarm::CallBack() {// �g���ʪ����_CPU
    Interrupt *interrupt = kernel->interrupt;
    Scheduler * scheduler = kernel->scheduler;
    MachineStatus status = interrupt->getStatus();
    bool woken = scheduler->anyThreadWoken(); // �۩w�q�p�ƾ�++�F�ˬd�O�_��thread�w�g��v�����A�i�H��^Ready Queue
    kernel->currentThread->setPriority(kernel->currentThread->getPriority() - 1);
    if (status == IdleMode && !woken && scheduler->sleepingListEmpty()) {// is it time to quit?
        if (!interrupt->AnyFutureInterrupts()) {// ������b�ƶ���interrupts�ܡH
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
//      �o�Ouser program�I�sSleep(int)��A�g�Lexception.cc���i�J�I
//
//      �|���e���b���檺thread�O���U�Өðe�J��v���A
//
//----------------------------------------------------------------------
void Alarm::WaitUntil(int x) {
    //�����_�A������Lthread����j��i�J(preempt)�A�簣��thread
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    //�N�ثe�n���檺thread��J��v
    Thread* t = kernel->currentThread;
    cout << "Alarm::WaitUntil go sleep" << endl;
    kernel->scheduler->PutToSleep(t,x);
    //�}���_
    kernel->interrupt->SetLevel(oldLevel);
}