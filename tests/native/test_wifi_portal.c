/* Pure C validation/DNS checks: no radio, credentials or network access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/wifi_portal_helpers.h"
static size_t query(uint8_t *p,unsigned type)
{
    memset(p,0,512);p[0]=0x12;p[1]=0x34;p[2]=1;p[5]=1;
    size_t n=12;p[n++]=5;memcpy(p+n,"probe",5);n+=5;p[n++]=4;memcpy(p+n,"test",4);n+=4;p[n++]=0;
    p[n++]=type>>8;p[n++]=type&255;p[n++]=0;p[n++]=1;return n;
}
int main(void)
{
    assert(portal_profile_valid("Cottage","password123"));
    assert(portal_profile_valid("Open AP",""));
    assert(!portal_profile_valid("","password123"));
    assert(!portal_profile_valid("Cottage","short"));
    assert(!portal_profile_valid("bad\nSSID","password123"));
    char long_ssid[34];memset(long_ssid,'x',33);long_ssid[33]=0;
    assert(!portal_profile_valid(long_ssid,"password123"));
    char key[65];memset(key,'a',64);key[64]=0;assert(portal_profile_valid("Cottage",key));
    key[10]='z';assert(!portal_profile_valid("Cottage",key));
    uint8_t packet[512];size_t length=query(packet,1),reply=portal_dns_reply(packet,length,sizeof(packet));
    assert(reply==length+16&&packet[0]==0x12&&packet[1]==0x34&&packet[7]==1);
    assert(!memcmp(packet+reply-4,(uint8_t[]){192,168,4,1},4));
    length=query(packet,28);assert(portal_dns_reply(packet,length,sizeof(packet))==length&&packet[7]==0);
    length=query(packet,1);assert(!portal_dns_reply(packet,length,length+15));
    length=query(packet,1);packet[2]=0x81;assert(!portal_dns_reply(packet,length,sizeof(packet)));
    length=query(packet,1);packet[12]=0xc0;assert(!portal_dns_reply(packet,length,sizeof(packet)));
    length=query(packet,1);packet[12]=64;assert(!portal_dns_reply(packet,length,sizeof(packet)));
    length=query(packet,1);assert(!portal_dns_reply(packet,length-3,sizeof(packet)));
    puts("PASS: Wi-Fi profile limits and captive DNS A/AAAA/malformed-packet checks");
    return 0;
}
