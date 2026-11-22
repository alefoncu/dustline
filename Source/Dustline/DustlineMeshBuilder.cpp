#include "DustlineMeshBuilder.h"

static FVector AnyPerp(const FVector& A)
{
	const FVector R = FMath::Abs(A.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
	return FVector::CrossProduct(A, R).GetSafeNormal();
}

void FDustlineMeshBuilder::Tri(int32 Id, const FVector& A0, const FVector& B0, const FVector& C0, const FVector& NA0, const FVector& NB0, const FVector& NC0,
	const FVector2D& UA, const FVector2D& UB, const FVector2D& UC)
{
	FSec& Sec = S(Id);
	FVector A = Xf.TransformPosition(A0), B = Xf.TransformPosition(B0), C = Xf.TransformPosition(C0);
	FVector NA = Xf.TransformVectorNoScale(NA0), NB = Xf.TransformVectorNoScale(NB0), NC = Xf.TransformVectorNoScale(NC0);
	FVector2D Ub = UB, Uc = UC;
	FVector Nb = NB, Nc = NC;
	const FVector Ng = FVector::CrossProduct(B - A, C - A);
	if (FVector::DotProduct(Ng, NA + NB + NC) < 0.f) { Swap(B, C); Swap(Nb, Nc); Swap(Ub, Uc); }
	const int32 Base = Sec.V.Num();
	const FVector Ts[3] = { AnyPerp(NA), AnyPerp(Nb), AnyPerp(Nc) };
	const FVector Vs[3] = { A, B, C }, Ns[3] = { NA, Nb, Nc };
	const FVector2D Us[3] = { UA, Ub, Uc };
	for (int32 i = 0; i < 3; ++i)
	{
		Sec.V.Add(Vs[i]); Sec.N.Add(Ns[i].GetSafeNormal()); Sec.UV.Add(Us[i]); Sec.T.Add(FProcMeshTangent(Ts[i], false)); Sec.C.Add(FLinearColor::White);
		Sec.I.Add(Base + i);
	}
}

void FDustlineMeshBuilder::QuadN(int32 Id, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& ND,
	const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD)
{
	Tri(Id, A, B, C, NA, NB, NC, UA, UB, UC);
	Tri(Id, A, C, D, NA, NC, ND, UA, UC, UD);
}

void FDustlineMeshBuilder::Quad(int32 Id, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2D& US)
{
	const FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
	QuadN(Id, A, B, C, D, N, N, N, N, FVector2D(0, 0), FVector2D(US.X, 0), FVector2D(US.X, US.Y), FVector2D(0, US.Y));
}

void FDustlineMeshBuilder::Box(int32 Id, const FVector& C, const FVector& H)
{
	const FVector P[8] = {
		C + FVector(-H.X, -H.Y, -H.Z), C + FVector(H.X, -H.Y, -H.Z), C + FVector(H.X, H.Y, -H.Z), C + FVector(-H.X, H.Y, -H.Z),
		C + FVector(-H.X, -H.Y, H.Z),  C + FVector(H.X, -H.Y, H.Z),  C + FVector(H.X, H.Y, H.Z),  C + FVector(-H.X, H.Y, H.Z) };
	auto F = [&](int a, int b, int c, int d, const FVector& N) { QuadN(Id, P[a], P[b], P[c], P[d], N, N, N, N); };
	F(0, 1, 2, 3, FVector(0, 0, -1)); F(4, 5, 6, 7, FVector(0, 0, 1));
	F(0, 1, 5, 4, FVector(0, -1, 0)); F(3, 2, 6, 7, FVector(0, 1, 0));
	F(0, 3, 7, 4, FVector(-1, 0, 0)); F(1, 2, 6, 5, FVector(1, 0, 0));
}

void FDustlineMeshBuilder::Cylinder(int32 Id, const FVector& Base, const FVector& AxisIn, float R, float Len, int32 Segs, bool bCaps)
{
	const FVector Ax = AxisIn.GetSafeNormal(); const FVector U = AnyPerp(Ax), V = FVector::CrossProduct(Ax, U);
	for (int32 i = 0; i < Segs; ++i)
	{
		const float a0 = 2 * PI * i / Segs, a1 = 2 * PI * (i + 1) / Segs;
		const FVector d0 = U * FMath::Cos(a0) + V * FMath::Sin(a0), d1 = U * FMath::Cos(a1) + V * FMath::Sin(a1);
		QuadN(Id, Base + d0 * R, Base + d1 * R, Base + Ax * Len + d1 * R, Base + Ax * Len + d0 * R, d0, d1, d1, d0,
			FVector2D((float)i / Segs, 0), FVector2D((float)(i + 1) / Segs, 0), FVector2D((float)(i + 1) / Segs, 1), FVector2D((float)i / Segs, 1));
		if (bCaps)
		{
			Tri(Id, Base, Base + d1 * R, Base + d0 * R, -Ax, -Ax, -Ax, FVector2D(.5, .5), FVector2D(.5 + .5 * FMath::Cos(a1), .5 + .5 * FMath::Sin(a1)), FVector2D(.5 + .5 * FMath::Cos(a0), .5 + .5 * FMath::Sin(a0)));
			Tri(Id, Base + Ax * Len, Base + Ax * Len + d0 * R, Base + Ax * Len + d1 * R, Ax, Ax, Ax, FVector2D(.5, .5), FVector2D(.5 + .5 * FMath::Cos(a0), .5 + .5 * FMath::Sin(a0)), FVector2D(.5 + .5 * FMath::Cos(a1), .5 + .5 * FMath::Sin(a1)));
		}
	}
}

void FDustlineMeshBuilder::Lathe(int32 Id, const FVector& Origin, const FVector& AxisIn, const TArray<FVector2D>& P, int32 Segs, float UScale)
{
	const int32 N = P.Num(); if (N < 2) return;
	const FVector Ax = AxisIn.GetSafeNormal(); const FVector U = AnyPerp(Ax), V = FVector::CrossProduct(Ax, U);
	TArray<float> Vc; Vc.SetNum(N); Vc[0] = 0; for (int32 k = 1; k < N; ++k) Vc[k] = Vc[k - 1] + (P[k] - P[k - 1]).Size();
	const float Tot = FMath::Max(Vc.Last(), 1e-3f);
	for (int32 i = 0; i < Segs; ++i)
	{
		const float a0 = 2 * PI * i / Segs, a1 = 2 * PI * (i + 1) / Segs;
		const FVector d0 = U * FMath::Cos(a0) + V * FMath::Sin(a0), d1 = U * FMath::Cos(a1) + V * FMath::Sin(a1);
		for (int32 k = 0; k < N - 1; ++k)
		{
			auto Nrm = [&](int32 idx, const FVector& d) {
				const FVector2D T0 = P[FMath::Min(idx + 1, N - 1)] - P[FMath::Max(idx - 1, 0)];   // (ds, dr)
				return (d * T0.X - Ax * T0.Y).GetSafeNormal();
			};
			const FVector p00 = Origin + Ax * P[k].X + d0 * P[k].Y, p01 = Origin + Ax * P[k + 1].X + d0 * P[k + 1].Y;
			const FVector p10 = Origin + Ax * P[k].X + d1 * P[k].Y, p11 = Origin + Ax * P[k + 1].X + d1 * P[k + 1].Y;
			QuadN(Id, p00, p10, p11, p01, Nrm(k, d0), Nrm(k, d1), Nrm(k + 1, d1), Nrm(k + 1, d0),
				FVector2D(UScale * i / Segs, Vc[k] / Tot), FVector2D(UScale * (i + 1) / Segs, Vc[k] / Tot), FVector2D(UScale * (i + 1) / Segs, Vc[k + 1] / Tot), FVector2D(UScale * i / Segs, Vc[k + 1] / Tot));
		}
	}
}

void FDustlineMeshBuilder::Tube(int32 Id, const TArray<FVector>& Path, float R, int32 Segs)
{
	const int32 N = Path.Num(); if (N < 2) return;
	FVector Prev = AnyPerp((Path[1] - Path[0]).GetSafeNormal());
	TArray<TArray<FVector>> Rings; TArray<TArray<FVector>> Dirs;
	for (int32 j = 0; j < N; ++j)
	{
		FVector T = (Path[FMath::Min(j + 1, N - 1)] - Path[FMath::Max(j - 1, 0)]).GetSafeNormal();
		FVector Uv = (Prev - T * FVector::DotProduct(Prev, T)).GetSafeNormal(); if (Uv.IsNearlyZero()) Uv = AnyPerp(T);
		const FVector Vv = FVector::CrossProduct(T, Uv); Prev = Uv;
		TArray<FVector> Ring, Dir;
		for (int32 i = 0; i < Segs; ++i) { const float a = 2 * PI * i / Segs; const FVector d = Uv * FMath::Cos(a) + Vv * FMath::Sin(a); Dir.Add(d); Ring.Add(Path[j] + d * R); }
		Rings.Add(Ring); Dirs.Add(Dir);
	}
	for (int32 j = 0; j < N - 1; ++j) for (int32 i = 0; i < Segs; ++i)
	{
		const int32 i2 = (i + 1) % Segs;
		QuadN(Id, Rings[j][i], Rings[j][i2], Rings[j + 1][i2], Rings[j + 1][i], Dirs[j][i], Dirs[j][i2], Dirs[j + 1][i2], Dirs[j + 1][i],
			FVector2D((float)i / Segs, (float)j / (N - 1)), FVector2D((float)(i + 1) / Segs, (float)j / (N - 1)), FVector2D((float)(i + 1) / Segs, (float)(j + 1) / (N - 1)), FVector2D((float)i / Segs, (float)(j + 1) / (N - 1)));
	}
}

void FDustlineMeshBuilder::Sphere(int32 Id, const FVector& C, float R, int32 Segs)
{
	TArray<FVector2D> Prof;
	for (int32 k = 0; k <= Segs; ++k) { const float t = PI * k / Segs; Prof.Add(FVector2D(-R * FMath::Cos(t), R * FMath::Sin(t))); }
	Lathe(Id, C, FVector::UpVector, Prof, Segs * 2);
}

static float SPow(float V, float E) { return FMath::Sign(V) * FMath::Pow(FMath::Abs(V), E); }

void FDustlineMeshBuilder::SuperBox(int32 Id, const FVector& C, const FVector& H, float E, int32 Segs)
{
	const int32 NU = FMath::Max(4, Segs), NV = FMath::Max(8, Segs * 2);
	auto P = [&](float u, float v, FVector& Pos, FVector& Nrm)
	{
		const float cu = FMath::Cos(u), su = FMath::Sin(u), cv = FMath::Cos(v), sv = FMath::Sin(v);
		Pos = C + FVector(H.X * SPow(cu, E) * SPow(cv, E), H.Y * SPow(cu, E) * SPow(sv, E), H.Z * SPow(su, E));
		Nrm = FVector(SPow(cu, 2.f - E) * SPow(cv, 2.f - E) / FMath::Max(H.X, 0.01f), SPow(cu, 2.f - E) * SPow(sv, 2.f - E) / FMath::Max(H.Y, 0.01f), SPow(su, 2.f - E) / FMath::Max(H.Z, 0.01f)).GetSafeNormal();
	};
	for (int32 i = 0; i < NU; ++i) for (int32 j = 0; j < NV; ++j)
	{
		const float u0 = -HALF_PI + PI * i / NU, u1 = -HALF_PI + PI * (i + 1) / NU, v0 = 2 * PI * j / NV, v1 = 2 * PI * (j + 1) / NV;
		FVector p00, p01, p10, p11, n00, n01, n10, n11;
		P(u0, v0, p00, n00); P(u0, v1, p01, n01); P(u1, v0, p10, n10); P(u1, v1, p11, n11);
		QuadN(Id, p00, p01, p11, p10, n00, n01, n11, n10, FVector2D((float)j / NV, (float)i / NU), FVector2D((float)(j + 1) / NV, (float)i / NU), FVector2D((float)(j + 1) / NV, (float)(i + 1) / NU), FVector2D((float)j / NV, (float)(i + 1) / NU));
	}
}

void FDustlineMeshBuilder::Prism(int32 Id, const FVector& Base, const FVector& AxisIn, float R, float Len, int32 Sides, float Phase)
{
	const FVector Ax = AxisIn.GetSafeNormal(); const FVector U = AnyPerp(Ax), V = FVector::CrossProduct(Ax, U);
	for (int32 i = 0; i < Sides; ++i)
	{
		const float a0 = Phase + 2 * PI * i / Sides, a1 = Phase + 2 * PI * (i + 1) / Sides, am = 0.5f * (a0 + a1);
		const FVector d0 = U * FMath::Cos(a0) + V * FMath::Sin(a0), d1 = U * FMath::Cos(a1) + V * FMath::Sin(a1), dm = U * FMath::Cos(am) + V * FMath::Sin(am);
		QuadN(Id, Base + d0 * R, Base + d1 * R, Base + Ax * Len + d1 * R, Base + Ax * Len + d0 * R, dm, dm, dm, dm);
		Tri(Id, Base, Base + d1 * R, Base + d0 * R, -Ax, -Ax, -Ax, FVector2D(.5, .5), FVector2D(1, 0), FVector2D(0, 0));
		Tri(Id, Base + Ax * Len, Base + Ax * Len + d0 * R, Base + Ax * Len + d1 * R, Ax, Ax, Ax, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
	}
}

void FDustlineMeshBuilder::TubeVar(int32 Id, const TArray<FVector>& Path, float R0, float R1, int32 Segs, bool bCaps)
{
	const int32 N = Path.Num(); if (N < 2) return;
	FVector Prev = AnyPerp((Path[1] - Path[0]).GetSafeNormal());
	TArray<TArray<FVector>> Rings, Dirs; TArray<FVector> Tans;
	for (int32 j = 0; j < N; ++j)
	{
		const FVector T = (Path[FMath::Min(j + 1, N - 1)] - Path[FMath::Max(j - 1, 0)]).GetSafeNormal(); Tans.Add(T);
		FVector Uv = (Prev - T * FVector::DotProduct(Prev, T)).GetSafeNormal(); if (Uv.IsNearlyZero()) Uv = AnyPerp(T);
		const FVector Vv = FVector::CrossProduct(T, Uv); Prev = Uv;
		const float R = FMath::Lerp(R0, R1, (float)j / (N - 1));
		TArray<FVector> Ring, Dir;
		for (int32 i = 0; i < Segs; ++i) { const float a = 2 * PI * i / Segs; const FVector d = Uv * FMath::Cos(a) + Vv * FMath::Sin(a); Dir.Add(d); Ring.Add(Path[j] + d * R); }
		Rings.Add(Ring); Dirs.Add(Dir);
	}
	for (int32 j = 0; j < N - 1; ++j) for (int32 i = 0; i < Segs; ++i)
	{
		const int32 i2 = (i + 1) % Segs;
		QuadN(Id, Rings[j][i], Rings[j][i2], Rings[j + 1][i2], Rings[j + 1][i], Dirs[j][i], Dirs[j][i2], Dirs[j + 1][i2], Dirs[j + 1][i]);
	}
	if (bCaps) for (int32 e = 0; e < 2; ++e)
	{
		const int32 j = e == 0 ? 0 : N - 1; const FVector Nn = e == 0 ? -Tans[0] : Tans[N - 1];
		for (int32 i = 0; i < Segs; ++i) Tri(Id, Path[j], Rings[j][i], Rings[j][(i + 1) % Segs], Nn, Nn, Nn, FVector2D(.5, .5), FVector2D(0, 0), FVector2D(1, 0));
	}
}

void FDustlineMeshBuilder::Torus(int32 Id, const FVector& Center, const FVector& Axis, float Major, float Minor, int32 Segs, int32 Sides)
{
	TArray<FVector2D> Prof;
	for (int32 k = 0; k <= Sides; ++k) { const float a = 2 * PI * k / Sides; Prof.Add(FVector2D(Minor * FMath::Sin(a), Major + Minor * FMath::Cos(a))); }
	Lathe(Id, Center, Axis, Prof, Segs);
}

void FDustlineMeshBuilder::Disc(int32 Id, const FVector& Center, const FVector& AxisIn, float Rin, float Rout, int32 Segs)
{
	const FVector Ax = AxisIn.GetSafeNormal(); const FVector U = AnyPerp(Ax), V = FVector::CrossProduct(Ax, U);
	for (int32 i = 0; i < Segs; ++i)
	{
		const float a0 = 2 * PI * i / Segs, a1 = 2 * PI * (i + 1) / Segs;
		const FVector d0 = U * FMath::Cos(a0) + V * FMath::Sin(a0), d1 = U * FMath::Cos(a1) + V * FMath::Sin(a1);
		QuadN(Id, Center + d0 * Rin, Center + d1 * Rin, Center + d1 * Rout, Center + d0 * Rout, Ax, Ax, Ax, Ax);
	}
}

TArray<FVector> FDustlineMeshBuilder::Smooth(const TArray<FVector>& P, int32 Sub)
{
	TArray<FVector> Out; const int32 N = P.Num(); if (N < 3) return P;
	for (int32 i = 0; i < N - 1; ++i)
	{
		const FVector P0 = P[FMath::Max(i - 1, 0)], P1 = P[i], P2 = P[i + 1], P3 = P[FMath::Min(i + 2, N - 1)];
		for (int32 s = 0; s < Sub; ++s)
		{
			const float t = (float)s / Sub, t2 = t * t, t3 = t2 * t;
			Out.Add(0.5f * ((2.f * P1) + (-P0 + P2) * t + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * t2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * t3));
		}
	}
	Out.Add(P.Last()); return Out;
}

void FDustlineMeshBuilder::Build(UProceduralMeshComponent* PMC, const TMap<int32, UMaterialInterface*>& Mats, bool bCollision)
{
	PMC->ClearAllMeshSections();
	for (auto& KV : Sections)
	{
		FSec& Sec = KV.Value; if (Sec.V.Num() == 0) continue;
		PMC->CreateMeshSection_LinearColor(KV.Key, Sec.V, Sec.I, Sec.N, Sec.UV, Sec.C, Sec.T, bCollision);
		if (UMaterialInterface* const* M = Mats.Find(KV.Key)) PMC->SetMaterial(KV.Key, *M);
	}
}
