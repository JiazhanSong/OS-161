#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>
 
/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */
 
/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */
 
/*
 * replace this with declarations of any synchronization and other variables you need here
 */

// declarations for functions
bool rightTurn(int car);
bool carCrash(int car1);
 
 
bool rightTurn(int car) {
  if (car/4 == north && car %4 == west) {
    return true;
  }
  else if (car/4 == south && car %4 == east) {
    return true;
  }
  else if (car/4 == east && car %4 == north) {
    return true;
  }
  else if (car/4 == west && car %4 == south) {
    return true;
  }
  return false;
}
 

static struct lock *intersectionLock;
static struct cv* northCV;
static struct cv* southCV;
static struct cv* eastCV;
static struct cv* westCV;
static int carCount[16] = {0};


bool carCrash(int car1) {
  for (int i=0; i<16; i++) {
    if (carCount[i]>0) {
      int car2 = i;
      KASSERT(i%5 != 0);
      if (car1/4 == car2/4) {    
      }
      else if ((car1/4 == car2 %4) && (car1 %4 == car2/4)) {
      }
      else if (car1 %4 != car2 %4 && (rightTurn(car1) || rightTurn(car2))) {
      }
      else {
        return true;
      }
    }
  }
  return false;
}

/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  intersectionLock = lock_create("intersectionLock");
  northCV = cv_create("northCV");
  southCV = cv_create("southCV");
  eastCV = cv_create("eastCV");
  westCV = cv_create("westCV");

  return;
}
 
/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  lock_destroy(intersectionLock);
  cv_destroy(northCV);
  cv_destroy(southCV);
  cv_destroy(eastCV);
  cv_destroy(westCV);
}
 
 
/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */
 
void
intersection_before_entry(Direction origin, Direction destination)
{
  
 
  int newCar = origin*4 + destination;
  KASSERT(newCar < 16);
 
  bool crash=true;
  lock_acquire(intersectionLock);
  while (crash) {
      crash=carCrash(newCar);

      if (crash) {
        if (destination == north) {
          cv_wait(northCV, intersectionLock);
        }
        else if (destination == south) {
          cv_wait(southCV, intersectionLock);
        }
        else if (destination == east) {
          cv_wait(eastCV, intersectionLock);
        }
        else {
          cv_wait(westCV, intersectionLock);
        }
      }
  }
  carCount[newCar]++;
  lock_release(intersectionLock);
}
 
 
/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */
 
void
intersection_after_exit(Direction origin, Direction destination)
{
  lock_acquire(intersectionLock);
  carCount[origin*4 + destination]--;
  if (rightTurn(curCar)) { // special case for removal of right turning cars
    if (destination == north) {
      cv_broadcast(northCV, intersectionLock);
    }
    else if (destination == south) {
      cv_broadcast(southCV, intersectionLock);
    }
    else if (destination == east) {
      cv_broadcast(eastCV, intersectionLock);
    }
    else {
      cv_broadcast(westCV, intersectionLock);
    }
  }
  else { // all other cars
    cv_broadcast(northCV, intersectionLock);
    cv_broadcast(southCV, intersectionLock);
    cv_broadcast(eastCV, intersectionLock);
    cv_broadcast(westCV, intersectionLock);
  }
  lock_release(intersectionLock);
}

