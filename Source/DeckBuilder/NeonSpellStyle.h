#pragma once
#include "CoreMinimal.h"
#include "NeonSpellStyle.generated.h"

UENUM(BlueprintType)
enum class ENeonSpellStyle : uint8 { Pulse, Prism, Halo, Photon, Kinetic, Barrier, ArcRelay, GhostLink, NeonCataclysm, OrbitRazor, RicochetSeal, StrobeRail, EchoPetals, EventHorizon, ShatterSigil, SatelliteChoir };

namespace NeonSpellPalette
{
    inline FLinearColor Color(ENeonSpellStyle Style)
    {
        const FLinearColor Colors[]={FLinearColor(0.06f,1.f,0.46f),FLinearColor(1.f,0.02f,0.5f),
            FLinearColor(0.58f,0.12f,1.f),FLinearColor(0.02f,0.68f,1.f),FLinearColor(1.f,0.62f,0.025f),FLinearColor(0.02f,1.f,0.78f),
            FLinearColor(0.02f,0.55f,1),FLinearColor(0.65f,0.2f,1),FLinearColor(1,0.01f,0.38f),FLinearColor(0.85f,1,0.08f),FLinearColor(0.12f,1,0.3f),
            FLinearColor(0.65f,0.9f,1),FLinearColor(1,0.22f,0.12f),FLinearColor(0.3f,0.04f,1),FLinearColor(1,0.38f,0.025f),FLinearColor(0.015f,1,0.72f)};
        return Colors[FMath::Clamp(int32(Style),0,15)];
    }
    inline ENeonSpellStyle ForCard(FName ID)
    {
        static const FName NewIDs[]={TEXT("ArcRelay"),TEXT("GhostLink"),TEXT("NeonCataclysm"),TEXT("OrbitRazor"),TEXT("RicochetSeal"),TEXT("StrobeRail"),TEXT("EchoPetals"),TEXT("EventHorizon"),TEXT("ShatterSigil"),TEXT("SatelliteChoir")};
        for(int32 I=0;I<10;++I) if(ID==NewIDs[I]) return ENeonSpellStyle(6+I);
        if(ID==TEXT("PulseDart")) return ENeonSpellStyle::Pulse;
        if(ID==TEXT("HaloNova")) return ENeonSpellStyle::Halo;
        if(ID==TEXT("PhotonLance")) return ENeonSpellStyle::Photon;
        if(ID==TEXT("KineticCards")) return ENeonSpellStyle::Kinetic;
        if(ID==TEXT("TalismanGuard")) return ENeonSpellStyle::Barrier;
        return ENeonSpellStyle::Prism;
    }
}
