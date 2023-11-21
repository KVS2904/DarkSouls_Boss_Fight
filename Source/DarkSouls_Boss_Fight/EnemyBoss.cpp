// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyBoss.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"


AEnemyBoss::AEnemyBoss()
{

}

void AEnemyBoss::StateChaseClose()
{
	const auto Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (const auto AIController = Cast<AAIController>(Controller))
	{
		const auto TargetDirection = Target->GetActorLocation() - GetActorLocation();
		const auto DotProduct = FVector::DotProduct(GetActorForwardVector(), TargetDirection.GetSafeNormal());
		if (Distance <= 900. && DotProduct >= .95)
		{
			if (Distance <= 300.)
			{
				Attack(false);
				return;
			}
		}
		else if (UGameplayStatics::GetTimeSeconds(GetWorld()) >=
			LongAttack_Timestamp + LongAttack_Cooldown &&
			AIController->LineOfSightTo(Target))
		{
			LongAttack_Timestamp = UGameplayStatics::GetTimeSeconds(GetWorld());
			LongAttack(true);
			return;
		}

		if (!AIController->IsFollowingAPath())
		{
			AIController->MoveToActor(Target);
		}
	}
}

void AEnemyBoss::LongAttack(bool Rotate)
{
	Super::Attack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	SetState(State::ATTACK);
	if (const auto AIController = Cast<AAIController>(Controller))
	{
		AIController->StopMovement();
	}

	if (Rotate)
	{
		auto Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0;
		const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(Rotation);
	}

	const auto Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	LongAttack_ForwardSpeed = Distance + 600.f;
	
	const auto RandomIndex = FMath::RandRange(0, LongAttackAnimations.Num() - 1);
	PlayAnimMontage(LongAttackAnimations[RandomIndex]);
}

void AEnemyBoss::MoveForward()
{
	const auto NewLocation = GetActorLocation() + GetActorForwardVector() *
		LongAttack_ForwardSpeed * GetWorld()->GetDeltaSeconds();
	SetActorLocation(NewLocation);
}

float AEnemyBoss::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageCauser == this)
	{
		return 0.f;
	}
	if (QuickHitsTaken == 0 || GetWorld()->GetTimeSeconds() - QuickHitsTimestamp <= 1.5f)
	{
		QuickHitsTaken++;
		QuickHitsTimestamp = GetWorld()->GetTimeSeconds();
		if (QuickHitsTaken >= 3)
		{
			bInterruptable = false;
		}
	}
	else
	{
		QuickHitsTaken = 0;
		bInterruptable = true;
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}