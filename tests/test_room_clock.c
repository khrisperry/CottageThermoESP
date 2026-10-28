#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "room_clock.h"
int main(void)
{
    assert(!room_clock_time_valid(0));
    assert(!room_clock_time_valid((time_t)1700000000));
    assert(room_clock_time_valid((time_t)1790000000));
    assert(!room_clock_idle(0,0));
    assert(!room_clock_idle(ROOM_CLOCK_IDLE_US+10,0));
    assert(!room_clock_idle(100,101));
    assert(!room_clock_idle(ROOM_CLOCK_IDLE_US,1));
    assert(room_clock_idle(ROOM_CLOCK_IDLE_US+1,1));
    struct tm t={0};
    int digits[4]={0};bool pm=false;
    t.tm_hour=0;t.tm_min=0;
    room_clock_digits(&t,digits,&pm);
    assert(digits[0]==1&&digits[1]==2&&digits[2]==0&&digits[3]==0&&!pm);
    t.tm_hour=9;t.tm_min=5;
    room_clock_digits(&t,digits,&pm);
    assert(digits[0]==-1&&digits[1]==9&&digits[2]==0&&digits[3]==5&&!pm);
    t.tm_hour=12;t.tm_min=30;
    room_clock_digits(&t,digits,&pm);
    assert(digits[0]==1&&digits[1]==2&&digits[2]==3&&digits[3]==0&&pm);
    t.tm_hour=23;t.tm_min=59;
    room_clock_digits(&t,digits,&pm);
    assert(digits[0]==1&&digits[1]==1&&digits[2]==5&&digits[3]==9&&pm);
    puts("PASS: 60-second idle threshold, no fake time before SNTP, 12-hour AM/PM digits");
    return 0;
}
