// Fill out your copyright notice in the Description page of Project Settings.


#include "Combatant.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ACombatant::ACombatant()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ACombatant::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ACombatant::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRotateTowardsTarget)
	{
		LookAtSmooth();
	}
}

// Called to bind functionality to input
void ACombatant::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ACombatant::Attack()
{
	bAttacking = true;
	bNextAttackReady = false;
	bAttackDamaging = false;

	AttackHitActors.Empty();
}

void ACombatant::AttackLunge()
{
	if (Target)
	{
		auto Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0.f;
		const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(Rotation);
	}

	const auto NextLocation = GetActorLocation() + (GetActorForwardVector() * LungeDistance);
	SetActorLocation(NextLocation, true);
}

void ACombatant::EndAttack()
{
	bAttacking = false;
	bNextAttackReady = false;
}

void ACombatant::SetAttackDamaging(bool Damaging)
{
	bAttackDamaging = Damaging;
}

void ACombatant::SetMovingForward(bool IsMovingForward)
{
	bMovingForward = IsMovingForward;
}

void ACombatant::SetMovingBackwards(bool IsMovingBackwards)
{
	bMovingBackwards = IsMovingBackwards;
}

void ACombatant::EndStumble()
{
	bStumbling = false;
}

void ACombatant::AttackNextReady()
{
	bNextAttackReady = true;
}

void ACombatant::LookAtSmooth()
{
	if (Target && bTargetLocked && !bAttacking &&
		!GetCharacterMovement()->IsFalling())
	{
		auto Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0.f;
		const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		const auto SmoothedRotation = FMath::Lerp(GetActorRotation(), Rotation, RotationSmoothing * GetWorld()->GetDeltaSeconds());
		LastRotationSpeed = SmoothedRotation.Yaw - GetActorRotation().Yaw;
		SetActorRotation(SmoothedRotation);
	}
}

float ACombatant::GetCurrentRotationSpeed()
{
	//!TODO Always bRotateTowardsTarget = true
	if (bRotateTowardsTarget) return LastRotationSpeed;
	return 0.0f;
}
