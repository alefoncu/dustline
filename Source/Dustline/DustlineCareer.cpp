#include "DustlineCareer.h"
#include "DustlineSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	struct FState { int32 Credits = DustlineCareer::StartCredits; TSet<FString> Owned; float DistAcc = 0.f; bool bLoaded = false; };
	FState& S() { static FState St; return St; }
	FString Path() { return FPaths::ProjectSavedDir() / TEXT("Garage") / TEXT("career.json"); }
	void Save()
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("credits"), S().Credits);
		TArray<TSharedPtr<FJsonValue>> A; for (const FString& K : S().Owned) A.Add(MakeShared<FJsonValueString>(K));
		O->SetArrayField(TEXT("owned"), A);
		FString Out; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		if (FJsonSerializer::Serialize(O, W)) FFileHelper::SaveStringToFile(Out, *Path());
	}
	void Load()
	{
		FState& St = S(); if (St.bLoaded) return; St.bLoaded = true;
		FString In; if (!FFileHelper::LoadFileToString(In, *Path())) return;
		TSharedPtr<FJsonObject> O; TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(In);
		if (!FJsonSerializer::Deserialize(R, O) || !O.IsValid()) return;
		double C = 0; if (O->TryGetNumberField(TEXT("credits"), C)) St.Credits = FMath::Max(0, (int32)C);
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("owned"), A)) for (const TSharedPtr<FJsonValue>& V : *A) St.Owned.Add(V->AsString());
	}
}

namespace DustlineCareer
{
	bool Enabled() { return FDustlineSettings::Get().bCareer; }
	int32 Credits() { Load(); return S().Credits; }
	void Earn(int32 Amount) { Load(); if (Amount <= 0) return; S().Credits += Amount; Save(); }
	bool Spend(int32 Amount)
	{
		Load(); if (Amount <= 0) return true;
		if (S().Credits < Amount) return false;
		S().Credits -= Amount; Save(); return true;
	}
	bool Owns(const FString& Key) { Load(); return S().Owned.Contains(Key); }
	void Grant(const FString& Key) { Load(); bool bNew = false; S().Owned.Add(Key, &bNew); if (bNew) Save(); }
	int32 AddDistance(float Meters)
	{
		Load(); FState& St = S(); St.DistAcc += FMath::Max(0.f, Meters);
		const int32 C = (int32)(St.DistAcc / MetersPerCredit); if (C <= 0) return 0;
		St.DistAcc -= C * MetersPerCredit; St.Credits += C;
		static double LastSave = 0.0; const double Now = FPlatformTime::Seconds();
		if (Now - LastSave > 10.0) { LastSave = Now; Save(); }   // no escribir el fichero en cada metro
		return C;
	}
	void Reset() { Load(); S().Credits = StartCredits; S().Owned.Reset(); S().DistAcc = 0.f; Save(); }
	FString Fmt(int32 C)
	{
		FString N = FString::FromInt(FMath::Abs(C)), O;
		for (int32 i = 0; i < N.Len(); ++i) { if (i > 0 && (N.Len() - i) % 3 == 0) O += TEXT("."); O += N[i]; }
		return (C < 0 ? TEXT("-") : TEXT("")) + O + TEXT(" cr");
	}
}
