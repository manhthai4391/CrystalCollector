

#include "MeleePlayer.h"

#include "GameFramework/Actor.h"
#include "Camera/CameraShakeBase.h"
#include "LegacyCameraShake.h"
#include "Camera/CameraShake.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MeleeEnemy.h"

AMeleePlayer::AMeleePlayer()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TargetLockDistance = 1500.0f;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	BaseTurnRate = 45.0f;
	BaseLookUpRate = 45.0f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Boom"));
	CameraBoom->SetupAttachment(RootComponent);

	// The camera follows at this distance behind the character
	CameraBoom->TargetArmLength = 500.0f;

	// Rotate the arm based on the controller
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));

	// Attach the camera to the end of the boom and let the boom adjust 
	// to match the controller orientation
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Camera does not rotate relative to arm
	FollowCamera->bUsePawnControlRotation = false;

	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(GetMesh(), "RightHandItem");
	Weapon->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

	EnemyDetectionCollider = CreateDefaultSubobject<USphereComponent>(TEXT("Enemy Detection Collider"));
	EnemyDetectionCollider->SetupAttachment(RootComponent);
	EnemyDetectionCollider->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	EnemyDetectionCollider->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn,
		ECollisionResponse::ECR_Overlap);
	EnemyDetectionCollider->SetSphereRadius(TargetLockDistance);

	Attacking = false;
	Rolling = false;
	TargetLocked = false;
	NextAttackReady = false;
	AttackDamaging = false;
	AttackIndex = 0;

	TargetFocusRange = 400.f;
	AutoRotatePlayerYawAngleThreshold = 30.f;

	PassiveMovementSpeed = 450.0f;
	CombatMovementSpeed = 250.0f;
	RollingSpeed = 600;
	GetCharacterMovement()->MaxWalkSpeed = PassiveMovementSpeed;

	CollectedGemstone = 0;
}

void AMeleePlayer::BeginPlay()
{
	Super::BeginPlay();

	EnemyDetectionCollider->OnComponentBeginOverlap.
		AddDynamic(this, &AMeleePlayer::OnEnemyDetectionBeginOverlap);
	EnemyDetectionCollider->OnComponentEndOverlap.
		AddDynamic(this, &AMeleePlayer::OnEnemyDetectionEndOverlap);

	TSet<AActor*> NearActors;
	EnemyDetectionCollider->GetOverlappingActors(NearActors);

	for (auto* EnemyActor : NearActors)
	{
		if (Cast<AMeleeEnemy>(EnemyActor) && IsValid(EnemyActor))
		{
			NearbyEnemies.Add(EnemyActor);
		}
	}
}

// Called every frame
void AMeleePlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FocusTarget();

	AnimationTick();

	if (IsValid(Target) && TargetLocked)
	{
		const FVector TargetDirection = Target->GetActorLocation() - GetActorLocation();

		if (TargetDirection.Size2D() > TargetFocusRange)
		{
			const FRotator Difference = UKismetMathLibrary::NormalizedDeltaRotator(Controller->GetControlRotation(), TargetDirection.ToOrientationRotator());

			if (FMath::Abs(Difference.Yaw) > AutoRotatePlayerYawAngleThreshold)
				AddControllerYawInput(DeltaTime * -Difference.Yaw * 0.5f);
		}
	}
	else if(!IsValid(Target) && TargetLocked)
	{
		SetInCombat(false);
	}
}

// Called to bind functionality to input
void AMeleePlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMeleePlayer::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMeleePlayer::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("CombatModeToggle", IE_Pressed, this,
		&AMeleePlayer::ToggleCombatMode);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this,
		&AMeleePlayer::Attack);
	PlayerInputComponent->BindAction("Roll", IE_Pressed, this,
		&AMeleePlayer::Roll);
	PlayerInputComponent->BindAction("CycleTarget+", IE_Pressed, this,
		&AMeleePlayer::CycleTargetClockwise);
	PlayerInputComponent->BindAction("CycleTarget-", IE_Pressed, this,
		&AMeleePlayer::CycleTargetCounterClockwise);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMeleePlayer::Jump);
}

void AMeleePlayer::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMeleePlayer::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMeleePlayer::Die()
{
	PlayAnimMontage(DeathAnimation);
}

void AMeleePlayer::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.f) && !Attacking && !Rolling && !Stumbling)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddMovementInput(Direction, Value);
	}

	InputDirection.X = Value;
}

void AMeleePlayer::MoveRight(float Value)
{
	if ((Controller != NULL) && (Value != 0.f) && !Attacking && !Rolling && !Stumbling)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(Direction, Value);
	}

	InputDirection.Y = Value;
}

void AMeleePlayer::AnimationTick()
{
	if (Rolling)
	{
		AddMovementInput(GetActorForwardVector(), RollingSpeed * GetWorld()->GetDeltaSeconds());
	}
	else if (Stumbling && MovingBackwards)
	{
		AddMovementInput(-GetActorForwardVector(), StumblingMovement * GetWorld()->GetDeltaSeconds());
	}
	else if (Attacking && AttackDamaging)
	{
		TSet<AActor*> WeaponOverlappingActors;
		Weapon->GetOverlappingActors(WeaponOverlappingActors);

		for (AActor* HitActor : WeaponOverlappingActors)
		{
			if (HitActor == this)
				continue;
			if (!AttackHitActors.Contains(HitActor))
			{
				float AppliedDamage = UGameplayStatics::ApplyDamage(HitActor, Damage, GetController(), this, UDamageType::StaticClass());

				if (AppliedDamage > 0.f)
				{
					AttackHitActors.Add(HitActor);
					GetWorld()->GetFirstPlayerController()->PlayerCameraManager->StartCameraShake(CameraShakeMinor);
				}
			}
		}
	}
}

void AMeleePlayer::CycleTarget(bool Clockwise)
{
	AActor* SuitableTarget = nullptr;
	if (IsValid(Target))
	{
		const FVector CameraLocation = Cast<APlayerController>(GetController())->PlayerCameraManager->GetCameraLocation();
		const FRotator TargetDirection = (Target->GetActorLocation() - CameraLocation).ToOrientationRotator();
		float BestYawDifference = INFINITY;
		for (const auto NearbyEnemy : NearbyEnemies)
		{
			if (NearbyEnemy == Target)
				continue;

			if (!IsValid(NearbyEnemy))
				continue;

			const FRotator NearbyEnemyDirection = (NearbyEnemy->GetActorLocation() - CameraLocation).ToOrientationRotator();
			const FRotator Difference = UKismetMathLibrary::NormalizedDeltaRotator(NearbyEnemyDirection, TargetDirection);

			if ((Clockwise && Difference.Yaw <= 0.f) || (!Clockwise && Difference.Yaw >= 0.f))
				continue;

			const float YawDifference = FMath::Abs(Difference.Yaw);
			if (YawDifference < BestYawDifference)
			{
				BestYawDifference = YawDifference;
				SuitableTarget = NearbyEnemy;
			}
		}
	}
	else
	{
		float BestDistance = INFINITY;
		for (const auto NearbyEnemy : NearbyEnemies)
		{
			if (IsValid(NearbyEnemy))
			{
				const float Distance = FVector::Dist(GetActorLocation(), NearbyEnemy->GetActorLocation());
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					SuitableTarget = NearbyEnemy;
				}
			}
		}
	}

	if (SuitableTarget)
	{
		Target = SuitableTarget;
		if (!TargetLocked)
		{
			SetInCombat(true);
		}
	}
}

void AMeleePlayer::CycleTargetClockwise()
{
	CycleTarget(true);
}

void AMeleePlayer::CycleTargetCounterClockwise()
{
	CycleTarget(false);
}

void AMeleePlayer::LookAtSmooth()
{
	if (!Rolling)
	{
		Super::LookAtSmooth();
	}
}

float AMeleePlayer::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageCauser == this || Rolling)
		return 0.f;

	if (CurrentHealth <= 0.f)
		return 0.f;

	EndAttack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	Stumbling = true;

	int32 AnimationIndex;
	
	do
	{
		AnimationIndex = FMath::RandRange(0, TakeHit_StumbleBackwards.Num() - 1);
	} while (AnimationIndex == LastStumbleIndex);

	PlayAnimMontage(TakeHit_StumbleBackwards[AnimationIndex]);
	UGameplayStatics::PlaySound2D(GetWorld(), hurtSFX, FMath::RandRange(0.8f, 1.f), FMath::RandRange(0.8f, 1.f));
	LastStumbleIndex = AnimationIndex;

	FVector Direction = DamageCauser->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.f;
	const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorRotation(Rotation);

	CurrentHealth -= DamageAmount;
	if (CurrentHealth <= 0)
	{
		Die();
	}

	return DamageAmount;
}

void AMeleePlayer::OnEnemyDetectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AMeleeEnemy>(OtherActor) && !NearbyEnemies.Contains(OtherActor))
	{
		NearbyEnemies.Add(OtherActor);
	}
}

void AMeleePlayer::OnEnemyDetectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<AMeleeEnemy>(OtherActor) && NearbyEnemies.Contains(OtherActor))
	{
		NearbyEnemies.Remove(OtherActor);
	}
}

void AMeleePlayer::Attack()
{
	if ((!Attacking || NextAttackReady) && !Rolling && !Stumbling && !GetCharacterMovement()->IsFalling())
	{
		Super::Attack();

		AttackIndex = AttackIndex >= Attacks.Num() ? 0 : AttackIndex;
		PlayAnimMontage(Attacks[AttackIndex++]);
		int attackSoundIndex = AttackIndex % 2 == 0 ? 0 : 1;
		UGameplayStatics::PlaySound2D(GetWorld(), attackSFX[attackSoundIndex], FMath::RandRange(0.8f, 1.f), FMath::RandRange(0.8f, 1.f));
	}
}

void AMeleePlayer::EndAttack()
{
	Super::EndAttack();
	AttackIndex = 0;
}

void AMeleePlayer::Roll()
{
	if (Rolling || Stumbling)
		return;
	EndAttack();

	if (!InputDirection.IsZero())
	{
		FRotator PlayerRotationZeroPitch = Controller->GetControlRotation();
		PlayerRotationZeroPitch.Pitch = 0.f;
		const FVector PlayerRight = FRotationMatrix(PlayerRotationZeroPitch).GetUnitAxis(EAxis::Y);
		const FVector PlayerForward = FRotationMatrix(PlayerRotationZeroPitch).GetUnitAxis(EAxis::X);
		const FVector DodgeDirection = PlayerForward * InputDirection.X + PlayerRight * InputDirection.Y;
		RollRotation = DodgeDirection.ToOrientationRotator();
	}
	else
	{
		RollRotation = GetActorRotation();
	}

	SetActorRotation(RollRotation);
	PlayAnimMontage(CombatRoll);
	Rolling = true;
}

void AMeleePlayer::StartRoll()
{
	Rolling = true;
	GetCharacterMovement()->MaxWalkSpeed = RollingSpeed;
	EndAttack();
}

void AMeleePlayer::EndRoll()
{
	Rolling = false;
	GetCharacterMovement()->MaxWalkSpeed = TargetLocked ? CombatMovementSpeed : PassiveMovementSpeed;
}

void AMeleePlayer::RollRotateSmooth()
{
	const FRotator SmoothedRotation = FMath::Lerp(GetActorRotation(), RollRotation, RotationSmoothing * GetWorld()->GetDeltaSeconds());
	SetActorRotation(SmoothedRotation);
}

void AMeleePlayer::FocusTarget()
{
	if (IsValid(Target))
	{
		if (DistanceToTarget() > TargetLockDistance)
		{
			ToggleCombatMode();
		}
	}
}

void AMeleePlayer::ToggleCombatMode()
{
	if (!TargetLocked)
	{
		CycleTarget();
	}
	else
	{
		SetInCombat(false);
	}
}

void AMeleePlayer::SetInCombat(bool InCombat)
{
	TargetLocked = InCombat;
	GetCharacterMovement()->bOrientRotationToMovement = !TargetLocked;
	GetCharacterMovement()->MaxWalkSpeed = TargetLocked ? CombatMovementSpeed : PassiveMovementSpeed;

	if (!TargetLocked)
	{
		Target = nullptr;
	}
}