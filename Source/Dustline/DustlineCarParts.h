#pragma once
#include "CoreMinimal.h"
#include "DustlineMeshBuilder.h"

// Sections / material slots shared by the procedural car parts
enum ESec { SPaint = 0, SGlass, SDark, SMetal, SRubber, SRim, SBlock, STurbo, SAccent, SLight, STail, SChrome, SAlu, SHose, SInterior, SCarbon, SRoof, SAmber, SNum };

/** Tyre + rim + brake for one wheel, centred on the origin, axle along +Y (SideSign = outward direction). Type: 0 road,1 all-terrain,2 mud,3 studded,4 balloon. */
void DustlineAddWheel(FDustlineMeshBuilder& B, float R, float Wd, int32 Type, float SideSign, bool bBrake = true);

/** Desplazamiento longitudinal de la carrocería v2 respecto al centro de la batalla (voladizos reales). */
float DustlineBodyXOff(int32 Style);
