#include "ParryComponent.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/SceneComponent.h"

IMPLEMENT_CLASS(UParryComponent, UActorComponent)

void UParryComponent::BeginPlay()
{

	AActor* Owner = GetOwner();
	if (!Owner) return;

	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (UBoxComponent* Box = Cast<UBoxComponent>(Comp))
		{
			if (!ScaleTarget)
			{
				ScaleTarget = Box;
				OriginalScale = Box->GetRelativeScale();
			}
			ParryDelegate.AddDynamic(Box, &UBoxComponent::OnParry);
		}
		else if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
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
	ParryDelegate.BroadCast();
}
