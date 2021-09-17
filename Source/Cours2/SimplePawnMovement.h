#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "SimplePawnMovement.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnApexReached);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLanding);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFloorLeft);



UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class COURS2_API USimplePawnMovement : public UPawnMovementComponent
{
	GENERATED_BODY()

	//Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//End UActorComponent Interface

	virtual bool IsExceedingMaxSpeed(float MaxSpeed) const override;

	
	protected:
	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;
	virtual void PerformFloorCheck();

	public:
	UPROPERTY(BlueprintAssignable, Category = SimplePawnMovement)
	FOnApexReached OnApexReached;
	UPROPERTY(BlueprintAssignable, Category = SimplePawnMovement)
	FOnLanding OnLanding;
	UPROPERTY(BlueprintAssignable, Category = SimplePawnMovement)
	FOnFloorLeft OnFloorLeft;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float MaxHorizontalSpeed = 4000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float MaxFallSpeed = 4000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float MaxRiseSpeed = 4000;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float TurnBoost = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float SpeedDecay = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float GravityAccel = 980;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	bool bActiveGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SimplePawnMovement)
	float FloorAngleTolerance = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SimplePawnMovement)
	float MaxStepHeight = 15;

	
	UPROPERTY(BlueprintReadOnly, Category=SimplePawnMovement)
	bool bTouchingFloor = false;

	protected:
	virtual void ApplyControlInputToVelocity(float DeltaTime);

	virtual FVector CorrectVelocity(FVector InputVelocity);

	virtual bool LimitWorldBounds();

	UPROPERTY(Transient)
	uint32 bPositionCorrected:1;

	
};