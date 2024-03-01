// Fill out your copyright notice in the Description page of Project Settings.


#include "MeleeCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AMeleeCharacter::AMeleeCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TargetLocked = false;
	AttackDamaging = false;
	Attacking = false;
	NextAttackReady = false;
	MovingForward = false;
	MovingBackwards = false;
	Stumbling = false;
	RotateTowardsTarget = true;
	bIsDead = false;
	RotationSmoothing = 5.f;
	LastRotationSpeed = 0.f;

	AttackForwardMovement = 70;
}

float AMeleeCharacter::DistanceToTarget()
{
	if (Target)
	{
		return FVector::Distance(GetActorLocation(), Target->GetActorLocation());
	}
	return -1.f;
}

// Called when the game starts or when spawned
void AMeleeCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

// Called every frame
void AMeleeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (RotateTowardsTarget)
	{
		LookAtSmooth();
	}
}

void AMeleeCharacter::Attack()
{
	Attacking = true;
	NextAttackReady = false;
	AttackDamaging = false;

	//empty array
	AttackHitActors.Empty();
}

void AMeleeCharacter::AttackLunge()
{
	if (Target != NULL)
	{
		FVector Direction = Target->GetActorLocation() - GetActorLocation();
		Direction = FVector(Direction.X, Direction.Y, 0);
		const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(Rotation);
	}

	const FVector NewLocation = GetActorLocation() + (GetActorForwardVector() * AttackForwardMovement);
	SetActorLocation(NewLocation, true);
}

void AMeleeCharacter::EndAttack()
{
	Attacking = false;
	NextAttackReady = false;
}

void AMeleeCharacter::SetAttackDamaging(bool Damaging)
{
	AttackDamaging = Damaging;
}

void AMeleeCharacter::SetMovingForward(bool IsMovingForward)
{
	MovingForward = IsMovingForward;
}

void AMeleeCharacter::SetMovingBackwards(bool IsMovingBackwards)
{
	MovingBackwards = IsMovingBackwards;
}

void AMeleeCharacter::EndStumble()
{
	Stumbling = false;
}

void AMeleeCharacter::AttackNextReady()
{
	NextAttackReady = true;
}

void AMeleeCharacter::Die()
{
}

void AMeleeCharacter::LookAtSmooth()
{
	if (Target != NULL && TargetLocked && !Attacking && !GetCharacterMovement()->IsFalling())
	{
		FVector Direction = Target->GetActorLocation() - GetActorLocation();
		Direction = FVector(Direction.X, Direction.Y, 0);
		
		const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();

		const FRotator SmoothedRotation = FMath::Lerp(GetActorRotation(), Rotation, RotationSmoothing * GetWorld()->DeltaTimeSeconds);

		LastRotationSpeed = LastRotationSpeed = SmoothedRotation.Yaw - GetActorRotation().Yaw;

		SetActorRotation(SmoothedRotation);
	}
}

float AMeleeCharacter::GetCurrentRotationSpeed()
{
	if (RotateTowardsTarget)
		return LastRotationSpeed;

	return 0.0f;
}
