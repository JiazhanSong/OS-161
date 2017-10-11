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
typedef struct Car {
  Direction origin;
  Direction destination;
} Car;
 
// declarations for functions
bool rightTurn(Car *car);
bool carCrash(Car *car1, Car *car2);
 
 
bool rightTurn(Car *car) {
  if (car->origin == north && car->destination == west) {
    return true;
  }
  else if (car->origin == south && car->destination == east) {
    return true;
  }
  else if (car->origin == east && car->destination == north) {
    return true;
  }
  else if (car->origin == west && car->destination == south) {
    return true;
  }
  return false;
}
 
bool carCrash(Car *car1, Car *car2) {
  if (car1->origin == car2->origin) {
    return false;
  }
  else if ((car1->origin == car2->destination) && (car1->destination == car2->origin)) {
    return false;
  }
  else if (car1->destination != car2->destination) {
    if (rightTurn(car1) || rightTurn(car2)) {
      return false;
    }
    return true;
  }
  return true;
}
 
struct array* a;
static struct lock *intersectionLock;
static struct cv* northCV;
static struct cv* southCV;
static struct cv* eastCV;
static struct cv* westCV;
 
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
  a = array_create();
  array_init(a);
 
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
  array_destroy(a);
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
  lock_acquire(intersectionLock);
 
  Car *newCar = kmalloc(sizeof(struct Car));
  newCar->origin = origin;
  newCar->destination = destination;
 
  bool crash=true;
  while (crash) {
    crash=false;
    for (unsigned int i=0; i<array_num(a); i++) {
      if (carCrash(newCar, array_get(a, i))) {
        crash=true;
        break;
      }
    }
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
  array_add(a, newCar, NULL);
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
 
  for (unsigned int i=0; i<array_num(a); i++) {
    Car *curCar = array_get(a, i);
    if (curCar->origin == origin && curCar->destination == destination) { 
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

      array_remove(a, i);
      break;
    }
  }
 
  lock_release(intersectionLock);
}
