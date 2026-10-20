#include <assert.h>
#include <stdio.h>
#include "../../touchscreen/main/room_feedback.h"
int main(void)
{
 room_optimistic_t o={0};room_optimistic_begin(&o,true,1000000);
 assert(room_optimistic_update(&o,"off",500000,1100000));
 assert(room_optimistic_update(&o,"off",2000000,3000000));
 assert(!room_optimistic_update(&o,"on",4000000,4000000));
 room_optimistic_begin(&o,false,5000000);
 assert(room_optimistic_update(&o,"on",6000000,9000000));
 assert(!room_optimistic_update(&o,"on",9000000,10000000));
 room_optimistic_begin(&o,true,11000000);
 assert(room_optimistic_update(&o,"on",10000000,12000000)); /* old cached match is not confirmation */
 room_laundry_tracker_t t={0};
 assert(!room_laundry_observe(&t,"Not In Use",true,1000000));assert(!t.completed);
 assert(!room_laundry_observe(&t,"Washing",true,2000000));
 assert(!room_laundry_observe(&t,"Rinsing",true,3000000));
 assert(room_laundry_observe(&t,"Not In Use",true,4000000));assert(t.completed&&!t.cycle_active);
 assert(!room_laundry_observe(&t,"Not In Use",true,5000000));assert(!t.completed);
 assert(!room_laundry_observe(&t,"Drying",true,6000000));
 assert(!room_laundry_observe(&t,"",false,7000000));assert(t.cycle_active&&!t.armed);
 assert(!room_laundry_observe(&t,"Not In Use",true,8000000));assert(t.completed&&!t.cycle_active);
 assert(!room_laundry_observe(&t,"Drying",true,9000000));
 assert(!room_laundry_observe(&t,"Not In Use",true,40000000)); /* stale gap */assert(t.completed);
 assert(!room_laundry_observe(&t,"Drying",true,41000000));
 assert(room_laundry_observe(&t,"Not In Use",true,42000000));
 puts("PASS: optimistic hold/ack/rollback and single laundry completion without boot/outage alerts");
 return 0;
}
