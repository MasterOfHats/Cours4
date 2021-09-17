#include "SimplePawnMovement.h"

void USimplePawnMovement::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PawnOwner || !UpdatedComponent)
	{
		return;
	}

	const FVector PreVel = Velocity; 
	const AController* Controller = PawnOwner->GetController();
	if (Controller && Controller->IsLocalController())
	{
		// apply input for local players but also for AI that's not following a navigation path at the moment
		if (Controller->IsLocalPlayerController() == true || Controller->IsFollowingAPath() == false || bUseAccelerationForPaths)
		{
			ApplyControlInputToVelocity(DeltaTime);
		}
		// if it's not player controller, but we do have a controller, then it's AI
		// (that's not following a path) and we need to limit the speed
		else if (IsExceedingMaxSpeed(0) == true)
		{
			Velocity = CorrectVelocity(Velocity);
		}

		LimitWorldBounds();
		bPositionCorrected = false;

		// Move actor
		FVector Delta = Velocity * DeltaTime;

		if (!Delta.IsNearlyZero(1e-6f))
		{
			const FVector OldLocation = UpdatedComponent->GetComponentLocation();
			const FQuat Rotation = UpdatedComponent->GetComponentQuat();

			FHitResult Hit(1.f);
			SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);

			if (Hit.IsValidBlockingHit())
			{
				HandleImpact(Hit, DeltaTime, Delta);
				// Try to slide the remaining distance along the surface.
				SlideAlongSurface(Delta, 1.f-Hit.Time, Hit.Normal, Hit, true);
			}

			// Update velocity
			// We don't want position changes to vastly reverse our direction (which can happen due to penetration fixups etc)
			if (!bPositionCorrected)
			{
				const FVector NewLocation = UpdatedComponent->GetComponentLocation();
				Velocity = ((NewLocation - OldLocation) / DeltaTime);
			}
		}
		
		// Finalize
		UpdateComponentVelocity();

		if(PreVel.Z > 0 && Velocity.Z < 0)
		{
			OnApexReached.Broadcast();
		}

		bool bPreFloor = bTouchingFloor;
		PerformFloorCheck();

		if(bPreFloor != bTouchingFloor)
		{
			if(bTouchingFloor == true)
			{
				OnLanding.Broadcast();
			}
			else
			{
				OnFloorLeft.Broadcast();
			}
		}
	
	}
}

void USimplePawnMovement::PerformFloorCheck()
{
	if(!UpdatedComponent->IsQueryCollisionEnabled())
	{
		return;
	}

	const float HeightCheckedAdjust = (2.4f + KINDA_SMALL_NUMBER);

	float FloorSweepTraceDist = FMath::Max(2.4f, MaxStepHeight + HeightCheckedAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;

	if(FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		USceneComponent* Component = PawnOwner->GetRootComponent();

		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if(!PrimComp)
		{
			bTouchingFloor = false;
		}

		FCollisionShape CollisionShape = PrimComp->GetCollisionShape();

		FVector Start = PawnOwner->GetActorLocation();
		FVector End = Start - FVector(0,0,FloorLineTraceDist);
		FHitResult OutHit;
		
		bool bBlockingHit = false;
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, PawnOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(FRotator(0,0,0)), CollisionChannel, CollisionShape, QueryParams, ResponseParam);


		if(bBlockingHit)
		{
			float Angle = FMath::Abs(FMath::Acos(FVector::DotProduct(FVector(0,0,1), OutHit.Normal)));
			Angle = FMath::RadiansToDegrees(Angle);

			if(Angle > FloorAngleTolerance)
			{
				bTouchingFloor = false;
			} else
			{
				bTouchingFloor = true;
			}
		} else
		{
			bTouchingFloor = false;
		}
	
	
		
	}
}

bool USimplePawnMovement::IsExceedingMaxSpeed(float MaxSpeed) const
{
	float MaxSpeedH = FMath::Max(0.f, MaxHorizontalSpeed);
	float MaxSpeedR = FMath::Max(0.f, MaxRiseSpeed);
	float MaxSpeedF = FMath::Max(0.f, MaxFallSpeed);
	
	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;

	
	if((Velocity * FVector(1,1,0)).SizeSquared() > FMath::Square(MaxSpeedH) * OverVelocityPercent) return true;
	if(Velocity.Z > MaxSpeedR * OverVelocityPercent) return true;
	if(Velocity.Z < -MaxSpeedF * OverVelocityPercent) return true;

	return false;
}

bool USimplePawnMovement::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit,
	const FQuat& NewRotationQuat)
{
	bPositionCorrected |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotationQuat);
	return bPositionCorrected;
}

void USimplePawnMovement::ApplyControlInputToVelocity(float DeltaTime)
{
	const FVector HAcceleration = FVector(GetPendingInputVector().X, GetPendingInputVector().Y, 0);
	const float VAcceleration = GetPendingInputVector().Z;
	const float OverVelocityPercent = 1.01f;

	float MaxSpeedH = FMath::Max(0.f, MaxHorizontalSpeed);
	float MaxSpeedR = FMath::Max(0.f, MaxRiseSpeed);
	float MaxSpeedF = FMath::Max(0.f, MaxFallSpeed);

	FVector HVel = Velocity * FVector(1,1,0);
	float VVel = Velocity.Z;

	const bool bExceedingMaxSpeedH = HVel.SizeSquared() > FMath::Square(MaxSpeedH) * OverVelocityPercent;
	// const bool bExceedingMaxSpeedV = VVel > MaxSpeedR * OverVelocityPercent && VVel < -MaxSpeedF * OverVelocityPercent;

	if(HAcceleration.SizeSquared() > 0.f || bExceedingMaxSpeedH)
	{
		// Apply change in velocity direction
		if (Velocity.SizeSquared() > 0.f)
		{
			// Change direction faster than only using acceleration, but never increase velocity magnitude.
			const float TimeScale = FMath::Clamp(DeltaTime * TurnBoost, 0.f, 1.f);
			Velocity = Velocity + (HAcceleration.GetSafeNormal() * HVel.Size() - HVel) * TimeScale;
		}
	} else
	{
		// Dampen velocity magnitude based on deceleration.
		if (Velocity.SizeSquared() > 0.f)
		{
			// Change direction faster than only using acceleration, but never increase velocity magnitude.
			const float TimeScale = FMath::Clamp(DeltaTime * SpeedDecay, 0.f, 1.f);
			Velocity = Velocity - HVel * TimeScale;
		}
	}

	Velocity += HAcceleration * DeltaTime;
	Velocity.Z += VAcceleration * DeltaTime;
	if (bActiveGravity) Velocity.Z -= GravityAccel * DeltaTime;

	HVel = Velocity * FVector(1,1,0);
	VVel = Velocity.Z;

	HVel = HVel.GetClampedToMaxSize(MaxHorizontalSpeed);
	FMath::Clamp(VVel, MaxFallSpeed, MaxRiseSpeed);

	Velocity = FVector(HVel.X, HVel.Y, VVel);

	ConsumeInputVector();
	
}

FVector USimplePawnMovement::CorrectVelocity(FVector InputVelocity)
{
	float MaxSpeedH = FMath::Max(0.f, MaxHorizontalSpeed);
	float MaxSpeedR = FMath::Max(0.f, MaxRiseSpeed);
	float MaxSpeedF = FMath::Max(0.f, MaxFallSpeed);
	
	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;

	FVector HVel = InputVelocity * FVector(1,1,0);
	float VVel = InputVelocity.Z;

	
	if((HVel.SizeSquared() > FMath::Square(MaxSpeedH) * OverVelocityPercent))
	{
		HVel = HVel.GetSafeNormal() * MaxSpeedH;
	}
	if(VVel > MaxSpeedR * OverVelocityPercent)
	{
		VVel = MaxSpeedR;
	}
	else if(VVel < -MaxSpeedF * OverVelocityPercent)
	{
		VVel = -MaxSpeedF;
	}

	return FVector(HVel.X, HVel.Y, VVel);
}

bool USimplePawnMovement::LimitWorldBounds()
{
	AWorldSettings* WorldSettings = PawnOwner ? PawnOwner->GetWorldSettings() : NULL;
	if (!WorldSettings || !WorldSettings->bEnableWorldBoundsChecks || !UpdatedComponent)
	{
		return false;
	}

	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	if ( CurrentLocation.Z < WorldSettings->KillZ )
	{
		Velocity.Z = FMath::Min(GetMaxSpeed(), WorldSettings->KillZ - CurrentLocation.Z + 2.0f);
		return true;
	}

	return false;
}
