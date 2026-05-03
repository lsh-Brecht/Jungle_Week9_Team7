#pragma once

#include "Component/ActorComponent.h"
#include "Math/Vector.h"

class USceneComponent;

struct FControllerMovementInput
{
	FVector LocalInput = FVector::ZeroVector;      // X Forward, Y Right, Z Up
	FVector WorldDirection = FVector::ZeroVector; // Normalized world movement direction
	FVector WorldDelta = FVector::ZeroVector;     // Immediate movement delta for simple movement components
	float DeltaTime = 0.0f;
	float MoveSpeed = 0.0f;
	float SpeedMultiplier = 1.0f;
};

//TODO : 해당 컴포넌트 베이스 역할을 하고 고유의 기능은 없기에 오브젝트에 부여할 수 없도록 바꿔야 합니다!

/**
 * 런타임(PIE, Game mode) 동안
 * USceneComponent를 움직이는 로직들의 베이스 클래스.
 * 실제 이동 로직은 자식 클래스에서 담당합니다.
 */
class UMovementComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UMovementComponent, UActorComponent)

	UMovementComponent() = default;
	~UMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	USceneComponent* GetUpdatedComponent() const;
	bool HasValidUpdatedComponent() const { return GetUpdatedComponent() != nullptr; }
	const FString& GetUpdatedComponentPath() const { return UpdatedComponentPath; }
	FString GetUpdatedComponentDisplayName() const;
	TArray<USceneComponent*> GetOwnerSceneComponents() const;
	bool ResolveUpdatedComponent();
	FString BuildUpdatedComponentPath(const USceneComponent* TargetComponent) const;
	void ClearUpdatedComponentIfMatches(const USceneComponent* RemovedComponent);
	virtual bool CanReceiveControllerInput() const { return bReceiveControllerInput; }
	virtual void AddMovementInput(const FVector& WorldDirection, float Scale = 1.0f);
	virtual bool ApplyControllerMovementInput(const FControllerMovementInput& Input);
	virtual const FVector& GetLastMovementInput() const { return LastMovementInput; }
	virtual const FVector& GetVelocity() const { return Velocity; }
	virtual bool HasMovementInput() const { return !LastMovementInput.IsNearlyZero(); }
	virtual bool IsMoving() const { return !Velocity.IsNearlyZero(); }
	virtual void ClearMovementInputState();
	int32 GetControllerInputPriority() const { return ControllerInputPriority; }

protected:
	void TryAutoRegisterUpdatedComponent();
	USceneComponent* FindUpdatedComponentByPath(const FString& InPath) const;

	USceneComponent* UpdatedComponent = nullptr; // 움직일 대상
	bool bAutoRegisterUpdatedComponent = true;
	FString UpdatedComponentPath;
	bool bReceiveControllerInput = false;
	int32 ControllerInputPriority = 0;
	FVector LastMovementInput = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};
