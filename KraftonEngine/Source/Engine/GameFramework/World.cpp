#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Collision/PrimitiveCollision.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Types/LODContext.h"
#include "Scripting/LuaScriptSubsystem.h"
#include "Runtime/ObjectPoolSystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Component/ActorComponent.h"
#include <algorithm>
#include "Profiling/Stats.h"

IMPLEMENT_CLASS(UWorld, UObject)


UWorld::~UWorld()
{
	if (PersistentLevel && !PersistentLevel->GetActors().empty())
	{
		EndPlay();
	}
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
	// UE의 CreatePIEWorldByDuplication 대응 (간소화 버전).
	// 새 UWorld를 만들고, 소스의 Actor들을 하나씩 복제해 NewWorld를 Outer로 삼아 등록한다.
	// AActor::Duplicate 내부에서 Dup->GetTypedOuter<UWorld>() 경유 AddActor가 호출되므로
	// 여기서는 World 단위 상태만 챙기면 된다.
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld)
	{
		return nullptr;
	}
	NewWorld->SetOuter(NewOuter);
	NewWorld->InitWorld(); // Partition/VisibleSet 초기화 — 이거 없으면 복제 액터가 렌더링되지 않음

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

UWorld* UWorld::DuplicateAs(EWorldType InWorldType) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld) return nullptr;

	NewWorld->SetWorldType(InWorldType);
	NewWorld->InitWorld();

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

void UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor || !PersistentLevel) return;

	// 다른 시스템이 이 Actor/Pawn/Camera를 가리키고 있으면 파괴 전에 먼저 끊는다.
	CleanupActorReferences(Actor);
	FObjectPoolSystem::Get().ForgetActor(Actor);
	Actor->SetPooledActorState(false, false);
	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		RemoveWorldPrimitivePickingBVH(Primitive);
		RemoveWorldCollisionBVH(Primitive);
	}

	if (APlayerController* Controller = Cast<APlayerController>(Actor))
	{
		auto It = std::find(PlayerControllers.begin(), PlayerControllers.end(), Controller);
		if (It != PlayerControllers.end())
		{
			PlayerControllers.erase(It);
		}
	}

	Actor->EndPlay();
	// Remove from actor list
	PersistentLevel->RemoveActor(Actor);

	Partition.RemoveActor(Actor);

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

bool UWorld::ReleaseActor(AActor* Actor)
{
	return FObjectPoolSystem::Get().ReleaseActor(Actor);
}

int32 UWorld::WarmUpActorPool(UClass* Class, int32 Count)
{
	return FObjectPoolSystem::Get().WarmUp(this, Class, Count);
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor || !PersistentLevel)
	{
		return;
	}

	PersistentLevel->AddActor(Actor);

	if (APlayerController* Controller = Cast<APlayerController>(Actor))
	{
		if (std::find(PlayerControllers.begin(), PlayerControllers.end(), Controller) == PlayerControllers.end())
		{
			PlayerControllers.push_back(Controller);
		}
	}

	InsertActorToOctree(Actor);
	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		InsertWorldPrimitivePickingBVH(Primitive);
		InsertWorldCollisionBVH(Primitive);
	}

	// PIE 중 Duplicate(Ctrl+D)나 SpawnActor로 들어온 액터에도 BeginPlay를 보장.
	if (bHasBegunPlay && !Actor->HasActorBegunPlay() && !Actor->IsPooledActorInactive())
	{
		Actor->BeginPlay();
	}
}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::InsertWorldPrimitivePickingBVH(UPrimitiveComponent* Primitive)
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.InsertObject(Primitive);
}

void UWorld::RemoveWorldPrimitivePickingBVH(UPrimitiveComponent* Primitive)
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.RemoveObject(Primitive);
}

void UWorld::UpdateWorldPrimitivePickingBVH(UPrimitiveComponent* Primitive)
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.UpdateObject(Primitive);
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(GetActors());
}

void UWorld::CollectWorldPrimitivePickingBVHDebugAABBs(TArray<FWorldPrimitivePickingBVH::FDebugAABB>& OutAABBs) const
{
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors());
	WorldPrimitivePickingBVH.CollectDebugAABBs(OutAABBs);
}

void UWorld::MarkWorldCollisionBVHDirty()
{
	WorldCollisionSystem.MarkDirty();
}

void UWorld::InsertWorldCollisionBVH(UPrimitiveComponent* Primitive)
{
	WorldCollisionSystem.InsertObject(Primitive);
}

void UWorld::RemoveWorldCollisionBVH(UPrimitiveComponent* Primitive)
{
	WorldCollisionSystem.RemoveObject(Primitive);
}

void UWorld::UpdateWorldCollisionBVH(UPrimitiveComponent* Primitive)
{
	WorldCollisionSystem.UpdateObject(Primitive);
}

void UWorld::BuildWorldCollisionBVHNow() const
{
	WorldCollisionSystem.BuildNow(GetActors());
}

void UWorld::CollectWorldCollisionBVHDebugAABBs(TArray<FWorldCollisionBVH::FDebugAABB>& OutAABBs) const
{
	WorldCollisionSystem.EnsureBuilt(GetActors());
	WorldCollisionSystem.CollectDebugAABBs(OutAABBs);
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	//혹시라도 BVH 트리가 업데이트 되지 않았다면 업데이트
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors());
	return WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor);
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);
}

void UWorld::UpdateCollision()
{
	WorldCollisionSystem.UpdateCollision();
}

void UWorld::ApplyCollisionDebugVisualization()
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || Actor->IsPooledActorInactive() || !Actor->IsActorCollisionEnabled()) continue;
		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (UShapeComponent* ShapeComp = Cast<UShapeComponent>(Primitive))
			{
				ShapeComp->SetDebugShapeColor(FColor::Green());
			}
		}
	}

	for (const FOverlapPairKey& Pair : WorldCollisionSystem.GetCurrentOverlaps())
	{
		if (UShapeComponent* ShapeA = Cast<UShapeComponent>(Pair.ComponentA))
		{
			ShapeA->SetDebugShapeColor(FColor::Red());
		}
		if (UShapeComponent* ShapeB = Cast<UShapeComponent>(Pair.ComponentB))
		{
			ShapeB->SetDebugShapeColor(FColor::Red());
		}
	}
}

FLODUpdateContext UWorld::PrepareLODContext(const UCameraComponent* Camera)
{
	const UCameraComponent* LODCamera = Camera ? Camera : GetViewCamera();
	if (!LODCamera) return {};

	const FVector CameraPos = LODCamera->GetWorldLocation();
	const FVector CameraForward = LODCamera->GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| LastLODUpdateCamera != LODCamera
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
		LastLODUpdateCamera = const_cast<UCameraComponent*>(LODCamera);
		LastFullLODUpdateCameraPos = CameraPos;
		LastFullLODUpdateCameraForward = CameraForward;
		bHasLastFullLODUpdateCameraPos = true;
	}

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
	PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	PersistentLevel->SetWorld(this);
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	if (PersistentLevel)
	{
		PersistentLevel->BeginPlay();
	}
}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);

	TickManager.Tick(this, DeltaTime, TickType);

	UpdateCollision();

	ApplyCollisionDebugVisualization();
}

void UWorld::SetActiveCamera(UCameraComponent* InCamera)
{
	ActiveCamera = IsAliveObject(InCamera) ? InCamera : nullptr;
}

UCameraComponent* UWorld::GetActiveCamera() const
{
	return IsAliveObject(ActiveCamera) ? ActiveCamera : nullptr;
}

void UWorld::SetViewCamera(UCameraComponent* InCamera)
{
	ViewCamera = IsAliveObject(InCamera) ? InCamera : nullptr;
}

UCameraComponent* UWorld::GetViewCamera() const
{
	if (IsAliveObject(ViewCamera))
	{
		return ViewCamera;
	}
	return GetActiveCamera();
}

bool UWorld::IsActorInWorld(const AActor* Actor) const
{
	if (!Actor || !PersistentLevel || !IsAliveObject(Actor))
	{
		return false;
	}
	const TArray<AActor*>& Actors = PersistentLevel->GetActors();
	return std::find(Actors.begin(), Actors.end(), Actor) != Actors.end();
}

bool UWorld::IsComponentInWorld(const UActorComponent* Component) const
{
	if (!Component || !IsAliveObject(Component))
	{
		return false;
	}
	AActor* Owner = Component->GetOwner();
	if (!IsActorInWorld(Owner))
	{
		return false;
	}
	const TArray<UActorComponent*>& Components = Owner->GetComponents();
	return std::find(Components.begin(), Components.end(), Component) != Components.end();
}

void UWorld::CleanupActorReferences(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (APlayerController* Controller : PlayerControllers)
	{
		if (!Controller || Controller == Actor)
		{
			continue;
		}

		if (Controller->GetPawn() == Actor)
		{
			Controller->UnPossess();
		}
		if (Controller->GetViewTarget() == Actor)
		{
			Controller->SetViewTarget(nullptr);
		}
	}

	if (ViewCamera && ViewCamera->GetOwner() == Actor)
	{
		ViewCamera = nullptr;
	}
	if (ActiveCamera && ActiveCamera->GetOwner() == Actor)
	{
		ActiveCamera = nullptr;
	}
	if (LastLODUpdateCamera && LastLODUpdateCamera->GetOwner() == Actor)
	{
		LastLODUpdateCamera = nullptr;
		bHasLastFullLODUpdateCameraPos = false;
	}
}

void UWorld::CleanupComponentReferences(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (ViewCamera == Component)
	{
		ViewCamera = nullptr;
	}
	if (ActiveCamera == Component)
	{
		ActiveCamera = nullptr;
	}
	if (LastLODUpdateCamera == Component)
	{
		LastLODUpdateCamera = nullptr;
		bHasLastFullLODUpdateCameraPos = false;
	}
}

UCameraComponent* UWorld::FindFirstCamera() const
{
	if (!PersistentLevel)
	{
		return nullptr;
	}

	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (!Actor)
		{
			continue;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
			{
				return Camera;
			}
		}
	}
	return nullptr;
}

APawn* UWorld::FindFirstPawn() const
{
	if (!PersistentLevel)
	{
		return nullptr;
	}

	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn;
		}
	}
	return nullptr;
}

APlayerController* UWorld::CreatePlayerController()
{
	APlayerController* Controller = SpawnActor<APlayerController>();
	if (Controller)
	{
		Controller->InitDefaultComponents();
	}
	return Controller;
}

APlayerController* UWorld::FindOrCreatePlayerController()
{
	if (APlayerController* Existing = GetPlayerController(0))
	{
		Existing->InitDefaultComponents();
		return Existing;
	}
	return CreatePlayerController();
}

void UWorld::AutoWirePlayerController(APlayerController* PreferredController)
{
	APlayerController* Controller = (PreferredController && IsActorInWorld(PreferredController))
		? PreferredController
		: FindOrCreatePlayerController();
	if (!Controller || !IsActorInWorld(Controller))
	{
		return;
	}

	if (!Controller->GetPawn())
	{
		if (APawn* Pawn = FindFirstPawn())
		{
			Controller->Possess(Pawn);
		}
	}

	if (!Controller->GetViewTarget())
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			Controller->SetViewTarget(Pawn);
		}
		else
		{
			UCameraComponent* Camera = GetActiveCamera();
			if (!Camera)
			{
				Camera = FindFirstCamera();
			}
			if (Camera)
			{
				Controller->SetViewTarget(Camera->GetOwner());
			}
		}
	}

	if (!GetActiveCamera())
	{
		if (UCameraComponent* Camera = ResolveGameplayViewCamera(Controller))
		{
			SetActiveCamera(Camera);
		}
	}

	SetViewCamera(ResolveGameplayViewCamera(Controller));
}

UCameraComponent* UWorld::ResolveGameplayViewCamera(APlayerController* PreferredController) const
{
	if (PreferredController && IsActorInWorld(PreferredController))
	{
		if (UCameraComponent* Camera = PreferredController->ResolveViewCamera())
		{
			return Camera;
		}
	}

	for (APlayerController* Controller : PlayerControllers)
	{
		if (!Controller || !IsActorInWorld(Controller))
		{
			continue;
		}
		if (UCameraComponent* Camera = Controller->ResolveViewCamera())
		{
			return Camera;
		}
	}

	if (UCameraComponent* Camera = GetActiveCamera())
	{
		return Camera;
	}

	return FindFirstCamera();
}

APlayerController* UWorld::GetPlayerController(int32 Index) const
{
	if (Index < 0)
	{
		return nullptr;
	}
	const size_t NativeIndex = static_cast<size_t>(Index);
	if (NativeIndex >= PlayerControllers.size())
	{
		return nullptr;
	}
	APlayerController* Controller = PlayerControllers[NativeIndex];
	return IsActorInWorld(Controller) ? Controller : nullptr;
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;
	TickManager.Reset();

	PlayerControllers.clear();
	ViewCamera = nullptr;
	ActiveCamera = nullptr;

	if (!PersistentLevel)
	{
		return;
	}

	FObjectPoolSystem::Get().ClearWorld(this);

	PersistentLevel->EndPlay();

	// Clear spatial partition while actors/components are still alive.
	// Otherwise Octree teardown can dereference stale primitive pointers during shutdown.
	Partition.Reset(FBoundingBox());

	PersistentLevel->Clear();
	MarkWorldPrimitivePickingBVHDirty();
	MarkWorldCollisionBVHDirty();

	// PersistentLevel은 CreateObject로 생성되었으므로 DestroyObject로 해제해야 alloc count가 맞음
	UObjectManager::Get().DestroyObject(PersistentLevel);
	PersistentLevel = nullptr;
}
