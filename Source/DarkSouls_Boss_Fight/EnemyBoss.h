// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "EnemyBoss.generated.h"

/**
 *
 */
UCLASS()
class DARKSOULS_BOSS_FIGHT_API AEnemyBoss : public AEnemyBase
{
	GENERATED_BODY()

public:

	AEnemyBoss();

	UPROPERTY(EditAnywhere, Category = "Animations")
		TArray<UAnimMontage*> LongAttackAnimations;

	float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser);

protected:

	void StateChaseClose();
	void LongAttack(bool Rotate = true);
	void MoveForward();

private:
	// long range jump attack

	UPROPERTY(EditAnywhere, Category = "Combat")
		float LongAttack_Cooldown = 5.f;
	float LongAttack_Timestamp = -LongAttack_Cooldown;
	float LongAttack_ForwardSpeed;

	// after x consecutive hits, the enemy cannot be interrupted
	int QuickHitsTaken;
	float QuickHitsTimestamp;
};
