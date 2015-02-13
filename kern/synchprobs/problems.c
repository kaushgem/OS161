/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include <spl.h>

//Stop light
struct lock* getQuardrantLock(unsigned long);

struct lock *male_lock;
struct lock *female_lock;
struct lock *matcher_lock;

struct lock *room_lock;

volatile int count;


/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

void whalemating_init() {

	male_lock = lock_create("male lock");
	if (male_lock == NULL) {
		panic("Can't create male_lock");
	}

	female_lock = lock_create("female lock");
	if (female_lock == NULL) {
		panic("Can't create female_lock");
	}

	matcher_lock = lock_create("matcher lock");
	if (matcher_lock == NULL) {
		panic("Can't create matcher_lock");
	}

	room_lock = lock_create("room lock");
	if (room_lock == NULL) {
		panic("Can't create room_lock");
	}

	count = 0;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	kfree(room_lock);
	kfree(male_lock);
	kfree(female_lock);
	kfree(matcher_lock);
  return;
}


void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	male_start();
	// Implement this function

//	lock_acquire(male_lock);
//	while(lock_is_acquired(room_lock)) {}
//	count++;
//	while(count < 3) {}
//	lock_acquire(room_lock);
//	lock_release(male_lock);
//	count--;
//	while(count > 0) {}
//	lock_release(room_lock);

	male_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	female_start();
	// Implement this function

//	lock_acquire(female_lock);
//	while(lock_is_acquired(room_lock)) {}
//	count++;
//	while(!lock_is_acquired(room_lock)) {}
//	lock_release(female_lock);
//	count--;

	female_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
	(void)which;

	matchmaker_start();
	// Implement this function

//	lock_acquire(matcher_lock);
//	while(lock_is_acquired(room_lock)) {}
//	count++;
//	while(!lock_is_acquired(room_lock)) {}
//	lock_release(matcher_lock);
//	count--;

	matchmaker_end();

	// 08 Feb 2012 : GWA : Please do not change this code. This is so that your
	// whalemating driver can return to the menu cleanly.
	V(whalematingMenuSemaphore);
	return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.


struct lock *lock0;
struct lock *lock1;
struct lock *lock2;
struct lock *lock3;


void stoplight_init() {

	lock0 = lock_create("lock0");
	if (lock0 == NULL) {
		panic("Can't create lock0");
	}

	lock1 = lock_create("lock1");
	if (lock1 == NULL) {
		panic("Can't create lock1");
	}

	lock2 = lock_create("lock2");
	if (lock2 == NULL) {
		panic("Can't create lock2");
	}

	lock3 = lock_create("lock3");
	if (lock3 == NULL) {
		panic("Can't create lock3");
	}

  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {

	kfree(lock0);
	kfree(lock1);
	kfree(lock2);
	kfree(lock3);

  return;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.

  long targetQuadrant = (direction + 3) % 4;

//  while(true) {
//	  int spl = splhigh();
//	  if(!lock_is_acquired(getQuardrantLock(direction))){
//		  lock_acquire(getQuardrantLock(direction));
//		  splx(spl);
//
//		  spl = splhigh();
//		  if(!lock_is_acquired(getQuardrantLock(targetQuadrant))){
//			  lock_acquire(getQuardrantLock(targetQuadrant));
//			  splx(spl);
//			  break;
//		  }
//		  else{
//			  lock_release(getQuardrantLock(direction));
//		  }
//
//	  }
//	  splx(spl);
//  }

  while(true) {
	  int spl = splhigh();
	  if(!lock_is_acquired(getQuardrantLock(direction)) &&
			  !lock_is_acquired(getQuardrantLock(targetQuadrant))  )
	  {
		  lock_acquire(getQuardrantLock(direction));
		  lock_acquire(getQuardrantLock(targetQuadrant));
		  break;
	  }
	  splx(spl);
  }

  inQuadrant(direction);
  inQuadrant(targetQuadrant);

  leaveIntersection();

  lock_release(getQuardrantLock(targetQuadrant));
  lock_release(getQuardrantLock(direction));

  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;
  
  long middleQuadrant = (direction + 3) % 4;
  long targetQuadrant = (direction + 2) % 4;

//  while(true) {
//	  int spl = splhigh();
//	  if(!lock_is_acquired(getQuardrantLock(direction))){
//		  lock_acquire(getQuardrantLock(direction));
//		  splx(spl);
//
//		  spl = splhigh();
//		  if(!lock_is_acquired(getQuardrantLock(middleQuadrant))){
//			  lock_acquire(getQuardrantLock(middleQuadrant));
//			  splx(spl);
//
//			  spl = splhigh();
//			  if(!lock_is_acquired(getQuardrantLock(targetQuadrant))){
//				  lock_acquire(getQuardrantLock(targetQuadrant));
//				  splx(spl);
//				  break;
//			  }else{
//				  lock_release(getQuardrantLock(middleQuadrant));
//				  lock_release(getQuardrantLock(direction));
//			  }
//		  }else{
//			  lock_release(getQuardrantLock(direction));
//		  }
//	  }
//	  splx(spl);
  //  }

  while(true) {
	  int spl = splhigh();
	  if(!lock_is_acquired(getQuardrantLock(direction)) &&
			  !lock_is_acquired(getQuardrantLock(middleQuadrant)) &&
			  !lock_is_acquired(getQuardrantLock(targetQuadrant))  )
	  {
		  lock_acquire(getQuardrantLock(direction));
		  lock_acquire(getQuardrantLock(middleQuadrant));
		  lock_acquire(getQuardrantLock(targetQuadrant));
		  break;
	  }
	  splx(spl);
  }

  inQuadrant(direction);
  inQuadrant(middleQuadrant);
  inQuadrant(targetQuadrant);

  leaveIntersection();

  lock_release(getQuardrantLock(targetQuadrant));
  lock_release(getQuardrantLock(middleQuadrant));
  lock_release(getQuardrantLock(direction));


  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.

  lock_acquire(getQuardrantLock(direction));

  inQuadrant(direction);
  leaveIntersection();

  lock_release(getQuardrantLock(direction));

  V(stoplightMenuSemaphore);
  return;
}

struct lock*
getQuardrantLock(unsigned long direction)
{
	switch(direction)
	{
	case 0:{
		return lock0;
		break;
	}
	case 1:{
		return lock1;
		break;
	}
	case 2:{
		return lock2;
		break;
	}
	case 3:{
		return lock3;
		break;
	}
	}
	return NULL;
}



