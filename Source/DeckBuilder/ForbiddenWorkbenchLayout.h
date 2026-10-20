#pragma once

/** Approved layout A in the HUD's 1600 x 900 logical coordinates. */
namespace ForbiddenWorkbench
{
    constexpr float HowToX=806,SettingsX=989,ScoresX=1178,CloseX=1367,NavY=31,NavW=172,NavH=35;
    constexpr float RiderX=265,RiderY=116,RiderW=163,RiderH=50,RiderPitch=57;
    constexpr float GridX=491,GridY=226,CardW=251,CardH=244,CardPitchX=262,CardPitchY=256;
    constexpr float PresetX=686,PresetY=119,PresetW=119,PresetH=37,PresetPitch=124;
    constexpr float SaveX=1106,EnterX=1285,ActionY=119,ActionH=39;
    constexpr float TabsX=491,TabsY=174,TabW=167,TabPitch=174,TabH=34;
    constexpr float ViewX=492,ViewY=794,ViewW=260,ViewH=25,PrevX=1368,NextX=1452,PageY=794,PageW=75;
    constexpr int CardsPerPage=8;
    constexpr float CardX(int I) {return GridX+(I%4)*CardPitchX;}
    constexpr float CardY(int I) {return GridY+(I/4)*CardPitchY;}
    constexpr bool Within(float X,float Y,float L,float T,float W,float H) {return X>=L && X<=L+W && Y>=T && Y<=T+H;}
}
