#include <assert.h>
#include <stdio.h>
#include "../controller/main/network_profile.h"
int main(void)
{
    network_profile_t p={.magic=NETWORK_PROFILE_MAGIC};
    assert(network_profile_valid(&p));
    strcpy(p.ssid,"Cottage Wi-Fi");assert(network_profile_valid(&p));
    strcpy(p.password,"short");assert(!network_profile_valid(&p));
    strcpy(p.password,"valid passphrase");assert(network_profile_valid(&p));
    network_profile_t next;
    assert(network_profile_patch(&p,"Cottage Wi-Fi","",false,&next)&&!strcmp(next.password,p.password));
    assert(!network_profile_patch(&p,"Another LAN","",false,&next));
    assert(!network_profile_patch(&p,"Another LAN","short",false,&next));
    assert(network_profile_patch(&p,"Another LAN","new password",false,&next)&&!strcmp(next.password,"new password"));
    assert(network_profile_patch(&p,"Open LAN","",true,&next)&&!*next.password);
    assert(network_profile_patch(&p,"","",false,&next)&&!*next.ssid&&!*next.password);
    p.password[3]='\n';assert(!network_profile_valid(&p));
    memset(p.password,'a',64);p.password[64]=0;assert(network_profile_valid(&p));
    p.password[63]='z';assert(!network_profile_valid(&p));
    p.password[0]=0;memset(p.ssid,'s',32);p.ssid[32]=0;assert(network_profile_valid(&p));
    p.ssid[32]='s';assert(!network_profile_valid(&p));
    p.ssid[0]=0;strcpy(p.password,"valid passphrase");assert(!network_profile_valid(&p));
    p.password[0]=0;p.magic=0;assert(!network_profile_valid(&p));
    puts("PASS: network profile bounds, termination, open/AP-only, passphrase and hex PSK validation");
}
