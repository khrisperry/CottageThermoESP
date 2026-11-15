#include <assert.h>
#include "room_link_diagnostic.h"
int main(void)
{
    rld_log_t log={0};
    rld_event_t event={.uptime_ms=100000,.error=-1,.http_status=0,
        .duration_ms=1800,.rssi=-47,.room_wifi=true,.station_associated=true};
    snprintf(event.operation,sizeof(event.operation),"link_http");
    snprintf(event.reason,sizeof(event.reason),"timeout");
    rld_record(&log,RLD_LINK_FAIL,&event,true);
    assert(log.counters[RLD_LINK_FAIL]==1&&log.count==1);
    event.uptime_ms+=10000;
    rld_record(&log,RLD_LINK_FAIL,&event,true);
    assert(log.counters[RLD_LINK_FAIL]==2&&log.count==1);
    event.uptime_ms+=30001;
    rld_record(&log,RLD_LINK_FAIL,&event,true);
    assert(log.count==2);
    snprintf(event.reason,sizeof(event.reason),"bad_reply");
    rld_record(&log,RLD_LINK_FAIL,&event,true);
    assert(log.count==3);
    rld_record(&log,RLD_LINK_OK,&event,false);
    assert(log.count==3&&log.counters[RLD_LINK_OK]==1);
    rld_record(&log,RLD_SWITCH_RADIO,&event,true);
    rld_record(&log,RLD_SWITCH_LAN,&event,true);
    assert(log.counters[RLD_SWITCH_RADIO]==1);
    assert(log.counters[RLD_SWITCH_LAN]==1);
    assert(log.count==5);
    for(int i=0;i<100;++i){event.uptime_ms+=61000;rld_record(&log,RLD_LEARN_FAIL,&event,true);}
    assert(log.count==RLD_CAPACITY);
    assert(log.counters[RLD_LEARN_FAIL]==100);
    assert(log.next<RLD_CAPACITY);
    return 0;
}
