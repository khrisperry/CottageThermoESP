#include <assert.h>
#include <stdio.h>
#include "room_link_policy.h"
int main(void)
{
    assert(room_link_needs_status(TL_TARGET,true));
    assert(room_link_needs_status(TL_AWAY,true));
    assert(room_link_needs_status(TL_TARGET,false));
    assert(room_link_needs_status(TL_AWAY,false));
    assert(!room_link_needs_status(TL_SAMPLE,true));
    assert(room_link_needs_status(TL_SAMPLE,false));
    assert(!room_link_may_replay_mutation(TL_TARGET));
    assert(!room_link_may_replay_mutation(TL_AWAY));
    printf("PASS: user commands require fresh status/lease and are never blindly retried\n");
    return 0;
}
