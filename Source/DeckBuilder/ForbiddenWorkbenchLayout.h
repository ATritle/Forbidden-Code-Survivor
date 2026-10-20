#pragma once

/** Compact rider-left / deck-right workbench in 1600 x 900 logical coordinates.
 * Rendering, hit testing and playback validation share these bounds. */
namespace ForbiddenWorkbench
{
    constexpr float HowToX=797,SettingsX=952,ScoresX=1107,CloseX=1262,NavY=59,NavW=145,NavH=30;
    constexpr float RiderX=311,RiderY=137,RiderW=144,RiderH=40,RiderPitch=47;
    constexpr float GridX=519,GridY=220,CardW=210,CardH=200,CardPitchX=226,CardPitchY=214;
    constexpr float PresetX=1025,PresetY=137,PresetW=119,PresetH=30,PresetPitch=124;
    constexpr float SaveX=1072,SaveW=132,EnterX=1217,EnterW=190,ActionY=746,ActionH=30;
    constexpr float TabsX=519,TabsY=181,TabW=167,TabPitch=180,TabH=28;
    constexpr float ViewX=519,ViewY=649,ViewW=238,ViewH=27,PrevX=1250,NextX=1332,PageY=649,PageW=75;
    constexpr float HideoutX=165,HideoutY=610,HideoutW=290,HideoutH=29;
    constexpr int CardsPerPage=8;
    constexpr float CardX(int I) {return GridX+(I%4)*CardPitchX;}
    constexpr float CardY(int I) {return GridY+(I/4)*CardPitchY;}
    constexpr bool Within(float X,float Y,float L,float T,float W,float H) {return X>=L && X<=L+W && Y>=T && Y<=T+H;}
}
