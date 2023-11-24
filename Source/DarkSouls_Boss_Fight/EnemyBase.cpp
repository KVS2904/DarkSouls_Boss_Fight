// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyBase.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"

AEnemyBase::AEnemyBase()
{
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), "RightHandItem");
	Weapon->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	ActiveState = State::IDLE;

	Target = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickStateMachine();
}

void AEnemyBase::TickStateMachine()
{
	switch (ActiveState)
	{
	case State::IDLE:
		StateIdle();
		break;
	case State::CHASE_CLOSE:
		StateChaseClose();
		break;
	case State::CHASE_FAR:
		StateChaseFar();
		break;
	case State::ATTACK:
		StateAttack();
		break;
	case State::STUMBLE:
		StateStumble();
		break;
	//case State::TAUNT:
	//	break;
	case State::DEAD:
		StateDead();
		break;
	}
}

void AEnemyBase::SetState(State NewState)
{
	if (ActiveState != State::DEAD)
	{
		ActiveState = NewState;
	}
}

void AEnemyBase::StateIdle()
{
	if (Target && FVector::Dist(Target->GetActorLocation(), GetActorLocation()) <= 1200.f)
	{
		bTargetLocked = true;
		SetState(State::CHASE_CLOSE);
	}
}

void AEnemyBase::StateChaseClose()
{
	const auto Distance = FVector::Distance(Target->GetActorLocation(), GetActorLocation());
	if (Distance <= 300.)
	{
		const auto TargetDirection = Target->GetActorLocation() - GetActorLocation();
		const auto DotProduct = FVector::DotProduct(GetActorForwardVector(),
			TargetDirection.GetSafeNormal());
		if (DotProduct > .95f && !bAttacking && !bStumbling)
		{
			Attack(false);
		}
	}
	else
	{
		const auto AIController = Cast<AAIController>(Controller);
		if (AIController && !AIController->IsFollowingAPath())
		{
			AIController->MoveToActor(Target);
		}
	}
}

void AEnemyBase::MoveForward()
{
	const auto NewLocation = GetActorLocation() + (GetActorForwardVector() * 500.f * GetWorld()->GetDeltaSeconds());
	SetActorLocation(NewLocation, true);
}

void AEnemyBase::Attack(bool Rotate)
{
	Super::Attack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	SetState(State::ATTACK);
	Cast<AAIController>(Controller)->StopMovement();

	if (Rotate)
	{
		auto Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0.;

		const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(Rotation);
	}

	int32 RandomIndex = FMath::RandRange(0, AttackAnimations.Num() - 1);
	PlayAnimMontage(AttackAnimations[RandomIndex]);
}

void AEnemyBase::AttackNextReady()
{
	Super::AttackNextReady();
}

void AEnemyBase::EndAttack()
{
	Super::EndAttack();
	SetState(State::CHASE_CLOSE);
}

void AEnemyBase::AttackLunge()
{
	Super::AttackLunge();
}

void AEnemyBase::StateChaseFar()
{
	if (FVector::Dist(Target->GetActorLocation(), GetActorLocation()) < 850.)
	{
		SetState(State::CHASE_CLOSE);
	}
}

void AEnemyBase::StateAttack()
{
	if (bAttackDamaging)
	{
		TSet<AActor*> OverlappingActors;
		Weapon->GetOverlappingActors(OverlappingActors);

		for (const auto OtherActor : OverlappingActors)
		{
			if (OtherActor == this) continue;

			if (!AttackHitActors.Contains(OtherActor))
			{
				const auto AppliedDamage = UGameplayStatics::ApplyDamage(OtherActor, 1.f, GetController(), this, UDamageType::StaticClass());
				if (AppliedDamage > 0.f)
				{
					AttackHitActors.Add(OtherActor);
				}
			}
		}
	}

	if (bMovingForward)
	{
		MoveForward();
	}
}

void AEnemyBase::StateStumble()
{
	if (bStumbling)
	{
		if (bMovingBackwards)
		{
			AddMovementInput(-GetActorForwardVector(), 10.f * GetWorld()->GetDeltaSeconds());
		}
	}
	else
	{
		SetState(State::CHASE_CLOSE);
	}
}

void AEnemyBase::StateTaunt()
{
}

void AEnemyBase::StateDead()
{
}

void AEnemyBase::FocusTarget()
{
	if(const auto AIController = Cast<AAIController>(GetController()))
	{
		AIController->SetFocus(Target);
	}
}

float AEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageCauser == this) return 0.f;
	if (!bInterruptable) return DamageAmount;
	EndAttack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	bStumbling = true;
	SetState(State::STUMBLE);
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

	auto Direction = DamageCauser->GetActorLocation() - GetActorLocation();
	Direction.Z = 0;

	const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorRotation(Rotation);

	return DamageAmount;
}

void AEnemyBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

