

#pragma once

#include "CoreMinimal.h"
#include "MeleeCharacter.h"
#include "MeleeEnemy.generated.h"

/**
 *
 */
UCLASS()
class GAS_RPG_API AMeleeEnemy : public AMeleeCharacter
{
	GENERATED_BODY()

public:

	AMeleeEnemy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UStaticMeshComponent* Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Finite State Machine")
	uint8 ActiveState;

	UPROPERTY(EditAnywhere, Category = "Finite State Machine")
	float ChaseCloseDistance;

	UPROPERTY(EditAnywhere, Category = "Finite State Machine")
	float ChaseFarDistance;

	UPROPERTY(EditAnywhere, Category = "Finite State Machine")
	float AttackDistance;

	UPROPERTY(EditAnywhere, Category = "Finite State Machine")
	float StumblingDuration;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveSpeed;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float StumblingMovement;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser);

	UPROPERTY(EditAnywhere, Category = "Animations")
	UAnimMontage* OverheadSmash;

	int LastStumbleIndex;

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void FocusTarget();

	// returns weapon subobject
	FORCEINLINE class UStaticMeshComponent* GetWeapon() const { return Weapon; }

protected:

	virtual void BeginPlay() override;

	virtual void TickStateMachine();

	void SetState(uint8 NewState);

	virtual void StateIdle();

	// state: actively trying to keep close and attack the target
	virtual void StateChaseClose();

	// state: engaged but not currently trying to attack (idle behavior)
	virtual void StateChaseFar();

	virtual void StateAttack();

	virtual void StateStumble();

	virtual void StateTaunt();

	virtual void StateDead();

	virtual void MoveForward();

	virtual void Attack(bool Rotate = true);

	void AttackNextReady();

	void EndAttack();

	virtual void AttackLunge();

	virtual void Die() override;

	UFUNCTION(BlueprintCallable, Category = "Death")
	virtual void OnDeathAnimFinishPlaying();

	UPROPERTY(EditAnywhere, Category = "Audio")
	TArray<USoundBase*> attackSFX;

	bool Interruptable;

	float StumblingTime;
};
