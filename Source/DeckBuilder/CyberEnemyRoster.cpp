#include "CyberEnemyRoster.h"
const TArray<FCyberEnemyProfile>& CyberEnemies::Roster()
{
    static const TArray<FCyberEnemyProfile> Data={
        {TEXT("WireThrall"),TEXT("WIRE THRALL"),0,36,150,210,0,10,34,1},
        {TEXT("RelayRunner"),TEXT("RELAY RUNNER"),0,28,145,310,0,9,34,1},
        {TEXT("ChromeVagrant"),TEXT("CHROME VAGRANT"),0,58,175,170,20,14,38,2},
        {TEXT("GlitchAcolyte"),TEXT("GLITCH ACOLYTE"),0,44,165,190,35,12,34,2},
        {TEXT("RiotBulwark"),TEXT("RIOT BULWARK"),1,240,235,150,75,22,48,5},
        {TEXT("CoilHarrier"),TEXT("COIL HARRIER"),1,180,215,275,105,20,42,5},
        {TEXT("NullBishop"),TEXT("NULL BISHOP"),2,720,295,155,180,30,58,10},
        {TEXT("RazorSeraph"),TEXT("RAZOR SERAPH"),2,560,280,260,210,28,52,10},
        {TEXT("FurnaceOni"),TEXT("FURNACE ONI"),2,1100,325,125,240,38,65,14},
        {TEXT("BroadcastSovereign"),TEXT("BROADCAST SOVEREIGN"),3,6000,520,115,360,32,90,40}
    };return Data;
}
int32 CyberEnemies::Select(float Seconds,float Roll)
{
    const float Weights[]={34,30,20,16,12,10,4,3,2};float Sum=0;
    for(int32 I=0;I<9;++I) if(Seconds>=Roster()[I].UnlockSeconds) Sum+=Weights[I];
    float Pick=FMath::Clamp(Roll,0.f,0.99999f)*Sum;
    for(int32 I=0;I<9;++I) if(Seconds>=Roster()[I].UnlockSeconds) {Pick-=Weights[I];if(Pick<0) return I;}
    return 0;
}
