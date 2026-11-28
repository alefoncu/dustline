#pragma once
#include "CoreMinimal.h"

/**
 * Modo carrera: las piezas del taller cuestan créditos, que se ganan explorando (puntos de interés) y conduciendo.
 * Lo comprado queda en propiedad por modelo (se puede volver a montar gratis). Sin el modo carrera todo está libre.
 * Se guarda en Saved/Garage/career.json.
 */
namespace DustlineCareer
{
	constexpr int32 StartCredits = 4000;
	constexpr int32 PoiReward = 1500;
	constexpr float MetersPerCredit = 4.f;

	bool Enabled();                         // ajuste "Modo carrera"
	int32 Credits();
	void Earn(int32 Amount);
	bool Spend(int32 Amount);               // false si no llega
	bool Owns(const FString& Key);
	void Grant(const FString& Key);
	/** Créditos por distancia: acumula metros y devuelve los créditos ganados en esta llamada. */
	int32 AddDistance(float Meters);
	void Reset();                           // vuelve al principio (créditos iniciales, sin piezas)
	FString Fmt(int32 Credits);             // "4.000 cr"
}
