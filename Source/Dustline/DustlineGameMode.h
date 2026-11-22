#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DustlineGameMode.generated.h"

UCLASS()
class DUSTLINE_API ADustlineGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADustlineGameMode();
	virtual void BeginPlay() override;
};
