#include <assert.h>
#include <stdio.h>
#include "../touchscreen/main/room_wifi_retry.h"
int main(void)
{
    bool connecting=true;int64_t due=20000000,started=1000000,now=6000000;
    assert(room_wifi_retry_action(connecting,now,started,due,true)==ROOM_WIFI_ABORT);
    room_wifi_retry_abort(&connecting,&due,now,true);
    /* No disconnect callback occurs. Repeated ticks wait rather than aborting
     * forever; a new connect is scheduled even after a long blocked period. */
    for(int n=0;n<80;++n){
        now=6000000+n*250000;
        assert(room_wifi_retry_action(connecting,now,started,due,true)==ROOM_WIFI_WAIT);
    }
    now=26000000;assert(room_wifi_retry_action(connecting,now,started,due,true)==ROOM_WIFI_CONNECT);
    connecting=true;started=now;
    assert(room_wifi_retry_action(connecting,now+4999999,started,due,true)==ROOM_WIFI_WAIT);
    assert(room_wifi_retry_action(connecting,now+5000000,started,due,true)==ROOM_WIFI_ABORT);
    room_wifi_retry_abort(&connecting,&due,now+5000000,true);
    assert(room_wifi_retry_action(connecting,due,started,due,true)==ROOM_WIFI_CONNECT);
    assert(room_wifi_retry_action(true,14999999,0,0,false)==ROOM_WIFI_WAIT);
    assert(room_wifi_retry_action(true,15000000,0,0,false)==ROOM_WIFI_ABORT);
    puts("PASS: Wi-Fi retry deadlines, no-event disconnect recovery, bounded radio retry windows");
}
