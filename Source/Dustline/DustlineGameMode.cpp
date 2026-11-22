#include "DustlineGameMode.h"
#include "DustlineCar.h"

ADustlineGameMode::ADustlineGameMode()
{
	DefaultPawnClass = ADustlineCar::StaticClass();
}
