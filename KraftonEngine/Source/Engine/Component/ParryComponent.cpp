#include "ParryComponent.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/SceneComponent.h"
#include "Sound/SoundManager.h"
IMPLEMENT_CLASS(UParryComponent, UActorComponent)

void UParryComponent::BeginPlay()
{

	AActor* Owner = GetOwner();
	if (!Owner) return;

	ParryDelegate.Add(
		[]()
		{
			FSoundManager::Get().PlayEffect(SoundEffect::Parry);
		}
	);

	// Find the best ScaleTarget (prefer BoxComponent)
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (UBoxComponent* Box = Cast<UBoxComponent>(Comp))
		{
			ScaleTarget = Box;
			break;
		}
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
		{
			if (!ScaleTarget)
			{
				ScaleTarget = Prim;
			}
		}
	}

	if (ScaleTarget)
	{
		if (UBoxComponent* Box = Cast<UBoxComponent>(ScaleTarget))
		{
			ParryDelegate.AddDynamic(Box, &UBoxComponent::OnParry);
		}
		else if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(ScaleTarget))
		{
			ParryDelegate.AddDynamic(Prim, &UPrimitiveComponent::OnParry);
		}
	}
}

void UParryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	if (!bIsParrying) return;

	CurrentParryTime += DeltaTime;
	if (CurrentParryTime >= ParryDuration)
	{
		if (ScaleTarget)
			ScaleTarget->SetRelativeScale(OriginalScale);
		bIsParrying = false;
		CurrentParryTime = 0.f;
	}
}

void UParryComponent::Parry()
{
	if (bIsParrying) return;
	bIsParrying = true;
	CurrentParryTime = 0.f;

	if (ScaleTarget)
	{
		OriginalScale = ScaleTarget->GetRelativeScale();
	}

	ParryDelegate.BroadCast();
}
