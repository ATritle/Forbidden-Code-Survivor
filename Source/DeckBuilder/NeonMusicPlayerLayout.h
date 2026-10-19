#pragma once

/** Logical 1600 x 900 geometry shared by rendering, input and playback verification. */
namespace NeonMusicLayout
{
    constexpr float DockLeft=270, DockTop=822, DockWidth=1060, DockHeight=78;
    constexpr float PlaylistLeft=910, PlaylistTop=618, PlaylistWidth=400, PlaylistHeight=200;
    constexpr int VisibleRows=4;
    constexpr float RowLeft=930, RowTop=660, RowWidth=346, RowHeight=34, RowPitch=37;
    constexpr float ScrollLeft=1288, ScrollHeight=145;
    constexpr float SeekLeft=818, SeekWidth=140, VolumeLeft=1018, VolumeWidth=90;

    constexpr bool ContainsPlaylist(float X,float Y)
    {
        return X>=PlaylistLeft && X<=PlaylistLeft+PlaylistWidth &&
               Y>=PlaylistTop && Y<=PlaylistTop+PlaylistHeight;
    }
}
