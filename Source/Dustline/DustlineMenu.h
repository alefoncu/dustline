#pragma once
#include "CoreMinimal.h"

class ADustlineCar;
class SWidget;
class SBox;

/** Menú principal, pausa, ajustes y HUD (todo en Slate, estilo Apple). Lo posee el coche. */
class FDustlineMenu : public TSharedFromThis<FDustlineMenu>
{
public:
	enum class EScreen : uint8 { None, Main, Pause, Settings };

	explicit FDustlineMenu(ADustlineCar* InCar) : Car(InCar) {}
	void Start(bool bShowMain);
	void Shutdown();

	void ShowMain();
	void ShowPause();
	void ShowSettings();
	void StartDriving();     // desde el menú principal
	void Resume();           // desde la pausa
	void OnEscape();
	void RestartHintTimer();
	bool IsOpen() const { return Screen != EScreen::None; }

	EScreen Screen = EScreen::None;
	EScreen Prev = EScreen::Main;
	int32 Page = 0;

	// Para capturas automáticas (-uidemo)
	void SetPage(int32 P);

private:
	TWeakObjectPtr<ADustlineCar> Car;
	TSharedPtr<SWidget> Current, HudWidget, FadeW;
	TSharedPtr<float> FadeA;
	void Flash();
	TSharedPtr<SBox> PageBox;
	double DriveStart = 0.0;

	void Present(EScreen S, TSharedRef<SWidget> W);
	void Clear();
	void SetUIMode(bool bUI);
	void CloseGarageTo(EScreen Back);
	void OpenGarageFrom(EScreen Back);
	TSharedRef<SWidget> BuildMain();
	TSharedRef<SWidget> BuildPause();
	TSharedRef<SWidget> BuildSettings();
	TSharedRef<SWidget> BuildPage(int32 P);
	TSharedRef<SWidget> BuildHud();
};
