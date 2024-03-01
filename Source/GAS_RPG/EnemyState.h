// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class EnemyState : uint8
{
	IDLE,					// Outside of combat
	CHASE_CLOSE,			// Combat, staying close to target
	CHASE_FAR,				// Combat, doesn't care about range
	ATTACK,					// In the process of attacking
	STUMBLE,				// Stumbling from being damaged/interrupted
	TAUNT,					// Emoting during a fight
	DEAD					// Dead
};
