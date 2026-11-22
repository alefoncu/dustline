#include "DustlineSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FDustlineSettings& FDustlineSettings::Get()
{
	static FDustlineSettings S; static bool bLoaded = false;
	if (!bLoaded) { bLoaded = true; S.Load(); }
	return S;
}
static FString SettingsPath() { return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("dustline_settings.json"); }
void FDustlineSettings::Load()
{
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *SettingsPath())) FJsonObjectConverter::JsonObjectStringToUStruct(Json, this, 0, 0);
}
void FDustlineSettings::Save() const
{
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(*this, Json)) FFileHelper::SaveStringToFile(Json, *SettingsPath());
}
void FDustlineSettings::ApplyGlobal() const
{
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) V->Set(RenderScale, ECVF_SetByGameSetting);
}

namespace DustlineGfx
{
void ReapplyCritical(UWorld* World)
{
	// Estos tres ajustes mantienen el rendimiento en un Mac de 8 GB; ninguna calidad los vuelve a encender.
	if (!GEngine) return;
	GEngine->Exec(World, TEXT("r.Shadow.Virtual.Enable 0"));
	GEngine->Exec(World, TEXT("r.DynamicGlobalIlluminationMethod 0"));
	GEngine->Exec(World, TEXT("r.ReflectionMethod 2"));
}
void SetQuality(UWorld* World, int32 Level)
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; if (!U) return;
	U->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3)); U->ApplySettings(false); U->SaveSettings();
	ReapplyCritical(World);
}
int32 GetQuality()
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; return U ? FMath::Clamp(U->GetOverallScalabilityLevel(), 0, 3) : 2;
}
void SetFullscreen(UWorld* World, bool bFull)
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; if (!U) return;
	const FIntPoint Desk = U->GetDesktopResolution();
	if (bFull) { U->SetFullscreenMode(EWindowMode::WindowedFullscreen); U->SetScreenResolution(Desk); }
	else { U->SetFullscreenMode(EWindowMode::Windowed); U->SetScreenResolution(FIntPoint(FMath::Min(1600, (int32)(Desk.X * 0.8f)), FMath::Min(900, (int32)(Desk.Y * 0.8f)))); }
	U->ApplySettings(false); U->SaveSettings(); ReapplyCritical(World);
}
bool IsFullscreen()
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; return U && U->GetFullscreenMode() != EWindowMode::Windowed;
}
void SetVSync(UWorld* World, bool bOn)
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; if (!U) return;
	U->SetVSyncEnabled(bOn); U->ApplySettings(false); U->SaveSettings(); ReapplyCritical(World);
}
bool GetVSync() { UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; return U && U->IsVSyncEnabled(); }
void SetFpsLimit(UWorld* World, int32 Limit)
{
	UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; if (!U) return;
	U->SetFrameRateLimit((float)Limit); U->ApplySettings(false); U->SaveSettings(); ReapplyCritical(World);
}
int32 GetFpsLimit() { UGameUserSettings* U = GEngine ? GEngine->GetGameUserSettings() : nullptr; return U ? FMath::RoundToInt(U->GetFrameRateLimit()) : 0; }
}
