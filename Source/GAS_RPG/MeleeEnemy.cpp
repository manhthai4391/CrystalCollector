// Fill out your copyright notice in the Description page of Project Settings.

#include "MeleeEnemy.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyState.h"

AMeleeEnemy::AMeleeEnemy()
{
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(GetMesh(), "RightHandItem");
	Weapon->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

	ChaseFarDistance = 1200.f;
	ChaseCloseDistance = 850.f;
	AttackDistance = 300.f;

	MoveSpeed = 500.f;

	StumblingMovement = 10.f;

	StumblingDuration = 1.0f;
	StumblingTime = 0.f;

	PrimaryActorTick.bCanEverTick = true;
	MovingForward = false;
	MovingBackwards = false;
	Interruptable = true;
	LastStumbleIndex = 0;
}

void AMeleeEnemy::BeginPlay()
{
	Super::BeginPlay();

	ActiveState = (uint8)EnemyState::IDLE;

	Target = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void AMeleeEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickStateMachine();
}

void AMeleeEnemy::TickStateMachine()
{
	switch (ActiveState)
	{
	case (uint8)EnemyState::IDLE:
		StateIdle();
		break;

	case (uint8)EnemyState::CHASE_CLOSE:
		StateChaseClose();
		break;

	case (uint8)EnemyState::CHASE_FAR:
		StateChaseFar();
		break;

	case (uint8)EnemyState::ATTACK:
		StateAttack();
		break;

	case (uint8)EnemyState::STUMBLE:
		StateStumble();
		break;

		/*case (uint8)State::TAUNT:

			break;*/
	case (uint8)EnemyState::DEAD:
		StateDead();
		break;

	default:
		break;
	}
}

void AMeleeEnemy::SetState(uint8 NewState)
{
	if (ActiveState != (uint8)EnemyState::DEAD)
	{
		ActiveState = NewState;
	}
}

void AMeleeEnemy::StateIdle()
{
	if (Target && DistanceToTarget() < ChaseCloseDistance)
	{
		TargetLocked = true;

		SetState((uint8)EnemyState::CHASE_CLOSE);
	}
}

void AMeleeEnemy::StateChaseClose()
{
	if (Target && DistanceToTarget() < AttackDistance)
	{
		FVector TargetDirection = Target->GetActorLocation() - GetActorLocation();

		float DotProduct = FVector::DotProduct(GetActorForwardVector(), TargetDirection.GetSafeNormal());

		//if facing the target
		if (DotProduct > 0.95f && !Attacking && !Stumbling)
		{
			Attack(false);
		}
	}
	else 
	{
		AAIController* AIController = Cast<AAIController>(Controller);

		if (!AIController->IsFollowingAPath())
		{
			AIController->MoveToActor(Target);
		}
	}
}

void AMeleeEnemy::StateChaseFar()
{
	if (DistanceToTarget() < ChaseCloseDistance)
	{
		SetState((uint8)EnemyState::CHASE_CLOSE);
	}
}

void AMeleeEnemy::StateAttack()
{
	if (AttackDamaging)
	{
		TSet<AActor*> OverlappingActors;
		Weapon->GetOverlappingActors(OverlappingActors);

		for (const auto OtherActor : OverlappingActors)
		{
			if (OtherActor == this)
				continue;
			if (!AttackHitActors.Contains(OtherActor))
			{
				const float AppliedDamage = UGameplayStatics::ApplyDamage(OtherActor, Damage, GetController(), this, UDamageType::StaticClass());
				if (AppliedDamage > 0.f)
				{
					AttackHitActors.Add(OtherActor);
				}
			}
		}
	}

	if (MovingForward)
	{
		MoveForward();
	}
}

void AMeleeEnemy::StateStumble()
{
	if (Stumbling)
	{
		if (MovingBackwards)
		{
			AddMovementInput(-GetActorForwardVector(), 10.f * GetWorld()->GetDeltaSeconds());
		}
	}
	else
	{
		SetState((uint8)EnemyState::CHASE_CLOSE);
	}
}

void AMeleeEnemy::StateTaunt()
{
}

void AMeleeEnemy::StateDead()
{
}

void AMeleeEnemy::FocusTarget()
{
	if (const auto AIController = Cast<AAIController>(GetController()))
	{
		AIController->SetFocus(Target);
	}
}

float AMeleeEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageCauser == this)
		return 0.f;

	if (CurrentHealth <= 0.f)
		return 0.f;

	if (!Interruptable) 
		return DamageAmount;

	EndAttack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	Stumbling = true;
	SetState((uint8)EnemyState::STUMBLE);
	if (const auto AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	int32 AnimationIndex;
	do
	{
		AnimationIndex = FMath::RandRange(0, TakeHit_StumbleBackwards.Num() - 1);
	} while (AnimationIndex == LastStumbleIndex);
	PlayAnimMontage(TakeHit_StumbleBackwards[AnimationIndex]);
	LastStumbleIndex = AnimationIndex;
	
	FVector Direction = DamageCauser->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.f;

	const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorRotation(Rotation);

	CurrentHealth -= DamageAmount;
	if (CurrentHealth <= 0.f)
	{
		Die();
	}

	return DamageAmount;
}

void AMeleeEnemy::MoveForward()
{
	const FVector NewLocation = GetActorLocation() + (GetActorForwardVector() * MoveSpeed * GetWorld()->GetDeltaSeconds());
	SetActorLocation(NewLocation, true);
}

void AMeleeEnemy::Attack(bool Rotate)
{
	Super::Attack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	SetState((uint8)EnemyState::ATTACK);
	Cast<AAIController>(Controller)->StopMovement();

	if (Rotate)
	{
		FVector Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0.f;

		const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(Rotation);
	}

	int32 RandomIndex = FMath::RandRange(0, AttackAnimations.Num() - 1);
	PlayAnimMontage(AttackAnimations[RandomIndex]);

	int attackSoundIndex = RandomIndex % 2 == 0 ? 0 : 1;
	UGameplayStatics::PlaySound2D(GetWorld(), attackSFX[attackSoundIndex], FMath::RandRange(0.8f, 1.f), FMath::RandRange(0.8f, 1.f));
}

void AMeleeEnemy::AttackNextReady()
{
	Super::AttackNextReady();
}

void AMeleeEnemy::EndAttack()
{
	Super::EndAttack();
	SetState((uint8)EnemyState::CHASE_CLOSE);
}

void AMeleeEnemy::AttackLunge()
{
	Super::AttackLunge();
}

void AMeleeEnemy::Die()
{
	PlayAnimMontage(DeathAnimation);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetState((uint8)EnemyState::DEAD);
}

void AMeleeEnemy::OnDeathAnimFinishPlaying()
{
	GetWorld()->DestroyActor(this);
}

void AMeleeEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

