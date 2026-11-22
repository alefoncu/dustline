#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/** Small helper to build procedural geometry (boxes, cylinders, lathes, tubes) grouped by material section. */
struct FDustlineMeshBuilder
{
	struct FSec
	{
		TArray<FVector> V; TArray<int32> I; TArray<FVector> N; TArray<FVector2D> UV; TArray<FProcMeshTangent> T; TArray<FLinearColor> C;
	};
	TMap<int32, FSec> Sections;
	FTransform Xf = FTransform::Identity;

	FSec& S(int32 Id) { return Sections.FindOrAdd(Id); }

	/** Adds a triangle with explicit normals/uvs (world = Xf applied). */
	void Tri(int32 Id, const FVector& A, const FVector& B, const FVector& C, const FVector& NA, const FVector& NB, const FVector& NC,
		const FVector2D& UA, const FVector2D& UB, const FVector2D& UC);
	void Quad(int32 Id, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2D& UVScale = FVector2D(1, 1));
	void QuadN(int32 Id, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& ND,
		const FVector2D& UA = FVector2D(0, 0), const FVector2D& UB = FVector2D(1, 0), const FVector2D& UC = FVector2D(1, 1), const FVector2D& UD = FVector2D(0, 1));

	void Box(int32 Id, const FVector& Center, const FVector& Half);
	/** Cylinder along Axis (unit), from Base to Base+Axis*Length. */
	void Cylinder(int32 Id, const FVector& Base, const FVector& Axis, float Radius, float Length, int32 Segs = 16, bool bCaps = true);
	/** Lathe: profile points (axial position, radius) revolved around Axis through Origin. */
	void Lathe(int32 Id, const FVector& Origin, const FVector& Axis, const TArray<FVector2D>& Profile, int32 Segs = 32, float UScale = 1.f);
	/** Round tube along a polyline. */
	void Tube(int32 Id, const TArray<FVector>& Path, float Radius, int32 Segs = 8);
	void Sphere(int32 Id, const FVector& Center, float Radius, int32 Segs = 12);
	/** Rounded box (superellipsoid). Exponent E in (0,1]: small = boxy, 1 = ellipsoid. */
	void SuperBox(int32 Id, const FVector& Center, const FVector& Half, float E = 0.22f, int32 Segs = 14);
	/** Flat-shaded prism (nuts, bolts, fins). */
	void Prism(int32 Id, const FVector& Base, const FVector& Axis, float Radius, float Length, int32 Sides = 6, float PhaseRad = 0.f);
	/** Tube with radius interpolated from R0 to R1 along the path. */
	void TubeVar(int32 Id, const TArray<FVector>& Path, float R0, float R1, int32 Segs = 10, bool bCaps = true);
	/** Torus ring around Axis through Center. */
	void Torus(int32 Id, const FVector& Center, const FVector& Axis, float Major, float Minor, int32 Segs = 24, int32 Sides = 8);
	/** Flat annulus / disc facing +Axis (inner radius may be 0). */
	void Disc(int32 Id, const FVector& Center, const FVector& Axis, float Rin, float Rout, int32 Segs = 24);
	/** Catmull-Rom subdivision of a control polyline. */
	static TArray<FVector> Smooth(const TArray<FVector>& Pts, int32 Sub = 6);

	void Build(UProceduralMeshComponent* PMC, const TMap<int32, UMaterialInterface*>& Mats, bool bCollision = false);
};
