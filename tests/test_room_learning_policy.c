#include <assert.h>
#include <math.h>
#include "room_learning_policy.h"

int main(void)
{
    assert(rl_half_step_bin(1.0f)==0);
    assert(rl_half_step_bin(1.5f)==1);
    assert(rl_half_step_bin(2.5f)==3);
    assert(rl_half_step_bin(3.0f)==4);
    assert(rl_half_step_bin(6.0f)==10);
    assert(rl_half_step_bin(2.25f)==-1);
    assert(rl_half_step_bin(NAN)==-1);
    assert(rl_half_step_bin(0.0f)==-1);
    assert(rl_half_step_bin(6.5f)==-1);
    assert(!rl_settled(6000000,1000000,6,0.5f));
    assert(!rl_settled(7000000,1000000,5,0.5f));
    assert(!rl_settled(7000000,1000000,6,1.6f));
    assert(rl_settled(7000000,1000000,6,1.5f));
    assert(!rl_settled(7000000,0,6,0.5f));
    return 0;
}
