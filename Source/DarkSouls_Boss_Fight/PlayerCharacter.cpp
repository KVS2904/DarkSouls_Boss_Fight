// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "EnemyBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "LegacyCameraShake.h"

// Sets default values
APlayerCharacter::APlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

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
	GetCharacterMovement()->MaxWalkSpeed = PassiveMovementSpeed;
}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	EnemyDetectionCollider->OnComponentBeginOverlap.
		AddDynamic(this, &APlayerCharacter::OnEnemyDetectionBeginOverlap);
	EnemyDetectionCollider->OnComponentEndOverlap.
		AddDynamic(this, &APlayerCharacter::OnEnemyDetectionEndOverlap);

	TSet<AActor*> NearActors;
	EnemyDetectionCollider->GetOverlappingActors(NearActors);
	for (auto* EnemyActor : NearActors)
	{
		if (Cast<AEnemyBase>(EnemyActor))
		{
			NearbyEnemies.Add(EnemyActor);
		}
	}
}

// Called every frame
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	FocusTarget();//should we toggle of combat mode
	if (bRolling)
	{
		AddMovementInput(GetActorForwardVector(), RollingDistance * GetWorld()->GetDeltaSeconds());
	}
	else if (bStumbling && bMovingBackwards)
	{
		AddMovementInput(-GetActorForwardVector(), MovingBackwardsDistance * GetWorld()->GetDeltaSeconds());
	}
	else if (bAttacking && bAttackDamaging)
	{
		TSet<AActor*> WeaponOverlappingActors;
		Weapon->GetOverlappingActors(WeaponOverlappingActors);
		for (const auto HitActor : WeaponOverlappingActors)
		{
			if (HitActor == this) continue;
			if (!AttackHitActors.Contains(HitActor))
			{
				const auto AppliedDamage = UGameplayStatics::ApplyDamage(HitActor, 1.f, GetController(), this, UDamageType::StaticClass());
				if (AppliedDamage > 0.f)
				{
					AttackHitActors.Add(HitActor);
					GetWorld()->GetFirstPlayerController()->
						PlayerCameraManager->StartCameraShake(CameraShakeMinor);
				}
			}
		}
	}

	if (Target && bTargetLocked)
	{
		const auto TargetDirection = Target->GetActorLocation() - GetActorLocation();
		if (TargetDirection.Size2D() > 400.f)
		{
			const auto Difference = UKismetMathLibrary::NormalizedDeltaRotator(
				Controller->GetControlRotation(),
				TargetDirection.ToOrientationRotator());

			if (FMath::Abs(Difference.Yaw) > 30.f)
			{
				AddControllerYawInput(DeltaTime * -Difference.Yaw * .5f);
			}
		}
	}
}

// Called to bind functionality to input
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("CombatModeToggle", IE_Pressed, this,
		&APlayerCharacter::ToggleCombatMode);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this,
		&APlayerCharacter::Attack);
	PlayerInputComponent->BindAction("Roll", IE_Pressed, this,
		&APlayerCharacter::Roll);
	PlayerInputComponent->BindAction("CycleTarget+", IE_Pressed, this,
		&APlayerCharacter::CycleTargetClockwise);
	PlayerInputComponent->BindAction("CycleTarget-", IE_Pressed, this,
		&APlayerCharacter::CycleTargetCounterClockwise);
}

void APlayerCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void APlayerCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APlayerCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.f && !bAttacking && !bRolling && !bStumbling)
	{
		const auto Rotation = Controller->GetControlRotation();
		const auto YawRotation = FRotator(0.f, Rotation.Yaw, 0.f);
		const auto Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
	InputDirection.X = Value;
}

void APlayerCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.f && !bAttacking && !bRolling && !bStumbling)
	{
		const auto Rotation = Controller->GetControlRotation();
		const auto YawRotation = FRotator(0.f, Rotation.Yaw, 0.f);
		const auto Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
	InputDirection.Y = Value;
}

void APlayerCharacter::CycleTarget(bool Clockwise)
{
	AActor* SuitableTarget = nullptr;
	if (Target)
	{
		const auto CameraLocation = Cast<APlayerController>(GetController())->PlayerCameraManager->GetCameraLocation();
		const auto TargetDirection = (Target->GetActorLocation() - CameraLocation).ToOrientationRotator();
		auto BestYawDifference = INFINITY;
		for (const auto NearEnemy : NearbyEnemies)
		{
			if (NearEnemy == Target) continue;

			const auto NearEnemyDirection = (NearEnemy->GetActorLocation() - CameraLocation).ToOrientationRotator();
			const auto Difference = UKismetMathLibrary::NormalizedDeltaRotator(NearEnemyDirection, TargetDirection);
			if ((Clockwise && Difference.Yaw <= 0.f) || (!Clockwise && Difference.Yaw >= 0.f)) continue;

			const auto YawDifference = FMath::Abs(Difference.Yaw);
			if (YawDifference < BestYawDifference)
			{
				BestYawDifference = YawDifference;
				SuitableTarget = NearEnemy;
			}
		}
	}
	else
	{
		auto BestDistance = INFINITY;
		for (const auto NearEnemy : NearbyEnemies)
		{
			const auto Distance = FVector::Dist(GetActorLocation(), NearEnemy->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				SuitableTarget = NearEnemy;
			}
		}
	}
	if (SuitableTarget)
	{
		Target = SuitableTarget;
		if (!bTargetLocked)
		{
			SetInCombat(true);
		}
	}
}

void APlayerCharacter::CycleTargetClockwise()
{
	CycleTarget(true);
}

void APlayerCharacter::CycleTargetCounterClockwise()
{
	CycleTarget(false);
}

void APlayerCharacter::LookAtSmooth()
{
	if (!bRolling)
	{
		Super::LookAtSmooth();
	}
}

float APlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageCauser == this || bRolling) return 0.f;
	EndAttack();
	SetMovingBackwards(false);
	SetMovingForward(false);
	bStumbling = true;

	int32 AnimationIndex;
	do
	{
		AnimationIndex = FMath::RandRange(0, TakeHit_StumbleBackwards.Num() - 1);

	} while (AnimationIndex == LastStumbleIndex);

	PlayAnimMontage(TakeHit_StumbleBackwards[AnimationIndex]);
	LastStumbleIndex = AnimationIndex;

	auto Direction = DamageCauser->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.f;
	const auto Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorRotation(Rotation);

	return DamageAmount;
}

void APlayerCharacter::OnEnemyDetectionBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AEnemyBase>(OtherActor) && !NearbyEnemies.Contains(OtherActor))
	{
		NearbyEnemies.Add(OtherActor);
	}
}

void APlayerCharacter::OnEnemyDetectionEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<AEnemyBase>(OtherActor) && NearbyEnemies.Contains(OtherActor))
	{
		NearbyEnemies.Remove(OtherActor);
	}
}

void APlayerCharacter::Attack()
{
	if ((!bAttacking || bNextAttackReady) && !bRolling && !bStumbling &&
		!GetCharacterMovement()->IsFalling())
	{
		Super::Attack();

		AttackIndex = AttackIndex >= Attacks.Num() ? 0 : AttackIndex;
		PlayAnimMontage(Attacks[AttackIndex++]);
	}
}

void APlayerCharacter::EndAttack()
{
	Super::EndAttack();
	AttackIndex = 0;
}

void APlayerCharacter::Roll()
{
	if (bRolling || bStumbling) return;
	//! TODO Maybe add SetAttackDamaging(false); ?
	EndAttack();
	if (!InputDirection.IsZero())
	{
		auto PlayerRotZeroPitch = Controller->GetControlRotation();
		PlayerRotZeroPitch.Pitch = 0.f;
		const auto PlayerRight = FRotationMatrix(PlayerRotZeroPitch).GetUnitAxis(EAxis::Y);
		const auto PlayerForward = FRotationMatrix(PlayerRotZeroPitch).GetUnitAxis(EAxis::X);
		const auto DodgeDir = PlayerForward * InputDirection.X + PlayerRight * InputDirection.Y;
		RollRotation = DodgeDir.ToOrientationRotator();
	}
	else
	{
		RollRotation = GetActorRotation();
	}

	SetActorRotation(RollRotation);
	PlayAnimMontage(CombatRoll);
	bRolling = true;
}

void APlayerCharacter::StartRoll()
{
	bRolling = true;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	EndAttack();
}

void APlayerCharacter::EndRoll()
{
	bRolling = false;
	GetCharacterMovement()->MaxWalkSpeed = bTargetLocked ? CombatMovementSpeed : PassiveMovementSpeed;
}

void APlayerCharacter::RollRotateSmooth()
{
	UE_LOG(LogTemp, Warning, TEXT("SMOOTH!!!"));
	const auto SmoothedRotation = FMath::Lerp(GetActorRotation(),
		RollRotation, RotationSmoothing * GetWorld()->GetDeltaSeconds());
	SetActorRotation(SmoothedRotation);
}

void APlayerCharacter::FocusTarget()
{
	if (Target)
	{
		if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) >= TargetLockDistance)
		{
			ToggleCombatMode();
		}
	}
}

void APlayerCharacter::ToggleCombatMode()
{
	if (!bTargetLocked)
	{
		CycleTarget();
	}
	else
	{
		SetInCombat(false);
	}
}

void APlayerCharacter::SetInCombat(bool InCombat)
{
	bTargetLocked = InCombat;
	GetCharacterMovement()->bOrientRotationToMovement = !bTargetLocked;
	GetCharacterMovement()->MaxWalkSpeed = bTargetLocked ?
		CombatMovementSpeed : PassiveMovementSpeed;

	if (!bTargetLocked)
	{
		Target = nullptr;
	}
}
