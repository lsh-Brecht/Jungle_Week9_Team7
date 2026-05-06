#pragma once

#include "Camera/CameraTypes.h"
#include "Core/EngineTypes.h"
#include "GameFramework/AActor.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class APlayerController;
class FArchive;
class UActorComponent;
class UCameraComponent;
class UCameraModifier;
class UCameraShakeModifier;
struct FCameraShakeParams;

struct FViewTarget
{
	AActor* Target = nullptr;
	UCameraComponent* Camera = nullptr;
	FCameraView POV;
};

class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)

	APlayerCameraManager();
	~APlayerCameraManager() override;

	void InitDefaultComponents() override;
	void EndPlay() override;

	// AActor로 직렬화될 때 쓰는 함수
	void Serialize(FArchive& Ar) override;

	// PlayerController 내부 상태로 저장할 때 쓰는 함수
	void SerializeCameraState(FArchive& Ar);

	void Initialize(APlayerController* InOwner);

	void SetActiveCamera(UCameraComponent* Camera, bool bBlend);
	void ClearActiveCamera();

	UCameraComponent* GetActiveCamera() const;
	UCameraComponent* GetOutputCamera() const { return OutputCameraComponent; }
	bool HasValidOutputCamera() const;
	UCameraComponent* GetOutputCameraIfValid() const;

	void UpdateCamera(float DeltaTime);
	void SnapToActiveCamera();

	void AddCameraModifier(UCameraModifier* Modifier);
	void RemoveCameraModifier(UCameraModifier* Modifier, bool bImmediate = false);
	void ClearCameraModifiers();
	const TArray<UCameraModifier*>& GetCameraModifiers() const { return ModifierList; }

	void RemapActorReferences(const TMap<uint32, uint32>& ActorUUIDRemap);
	void ClearCameraReferencesForActor(const AActor* Actor);
	void ClearCameraReferencesForComponent(const UActorComponent* Component);

	UCameraShakeModifier* StartCameraShake(const FCameraShakeParams& Params);
private:
	UCameraComponent* ResolveCameraReference(const FCameraComponentReference& Ref) const;
	FCameraComponentReference MakeCameraReference(UCameraComponent* Camera) const;

	FCameraView BlendViews(
		const FCameraView& From,
		const FCameraView& To,
		float Alpha,
		const FCameraTransitionSettings& Params) const;

	float EvaluateBlendAlpha(float RawAlpha, ECameraBlendFunction Function) const;
	void EnsureOutputCamera();

	void ApplyCameraModifiers(float DeltaTime, FCameraView& InOutView);
	void CleanupCameraModifiers();
	void SortCameraModifiers();

private:
	APlayerController* OwnerController = nullptr;

	// Skeleton 요구사항 쪽
	FColor FadeColor = FColor::Black();
	float FadeAmount = 0.0f;
	FVector2 FadeAlpha = FVector2(0.0f, 0.0f);
	float FadeTime = 0.0f;
	float FadeTimeRemaining = 0.0f;

	FName CameraStyle;
	FViewTarget ViewTarget;

	// 기존 카메라 상태
	FCameraComponentReference ActiveCameraRef; // uint32 OwnerActorUUID = 0; FString ComponentPath;
	FCameraComponentReference PendingCameraRef;

	UCameraComponent* ActiveCameraCached = nullptr;
	UCameraComponent* PendingCameraCached = nullptr;
	UCameraComponent* OutputCameraComponent = nullptr;

	FCameraView CurrentView;
	FCameraView BlendFromView;

	TArray<UCameraModifier*> ModifierList;

	float BlendElapsedTime = 0.0f;
	bool bIsBlending = false;
};