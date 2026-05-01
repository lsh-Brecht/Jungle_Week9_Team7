#include "LuaBindings.h"

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

#include "SolInclude.h"

#include "LuaHandles.h"

#include "Core/Log.h"
#include "Math/Vector.h"
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Component/ActorComponent.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Component/Collision/ShapeComponent.h"
#include "Component/Collision/SphereComponent.h"
#include "Component/Movement/PendulumMovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"

namespace
{
	template<typename TComponent>
	TComponent* FindComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (TComponent* TypeComponent = Cast<TComponent>(Component))
			{
				return TypeComponent;
			}
		}

		return nullptr;
	}

	template<typename TComponent>
	TComponent* EnsureComponent(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (TComponent* Existing = FindComponent<TComponent>(Actor))
		{
			return Existing;
		}

		return Actor->AddComponent<TComponent>();
	}
}

// Lua에 함수를 바인딩함
void RegisterLuaBindings(sol::state& Lua)
{
	RegisterFVectorBinding(Lua);
	RegisterFRotatorBinding(Lua);
	
	RegisterShapeComponentBinding(Lua);
	RegisterSphereComponentBinding(Lua);
	RegisterBoxComponentBinding(Lua);
	RegisterCapsuleComponentBinding(Lua);

	RegisterMovementComponentBinding(Lua);
	RegisterProjectileMovementComponentBinding(Lua);
	RegisterInterpToMovementComponentBinding(Lua);
	RegisterPendulumMovementComponentBinding(Lua);
	RegisterRotatingMovementComponentBinding(Lua);
	
	RegisterGameObjectBinding(Lua);
	RegisterDelegateBinding(Lua);
}

// Lua에 FVector를 등록
void RegisterFVectorBinding(sol::state& Lua)
{
	Lua.new_usertype<FVector>(
		"FVector",
		sol::constructors<FVector(), FVector(float, float, float)>(),

		"x",
		sol::property([](const FVector& V) { return V.X; }, [](FVector& V, float Value) { V.X = Value; }),

		"y",
		sol::property([](const FVector& V) { return V.Y; }, [](FVector& V, float Value) { V.Y = Value; }),

		"z",
		sol::property([](const FVector& V) { return V.Z; }, [](FVector& V, float Value) { V.Z = Value; }),

		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,

		"Length",
		[](const FVector& Self) { return Self.Length(); },

		"Normalized",
		[](const FVector& Self) { return Self.Normalized(); },

		"Dot",
		[](const FVector& Self, const FVector& Other) { return Self.Dot(Other); },

		"Cross",
		[](const FVector& Self, const FVector& Other) { return Self.Cross(Other); },

		sol::meta_function::addition,
		[](const FVector& A, const FVector& B) { return A + B; },

		sol::meta_function::subtraction,
		[](const FVector& A, const FVector& B) { return A - B; },

		sol::meta_function::multiplication,
		sol::overload([](const FVector& V, float Scalar) { return V * Scalar; }, [](float Scalar, const FVector& V) { return V * Scalar; }),

		sol::meta_function::division,
		[](const FVector& V, float Scalar) { return V / Scalar; }

	);

	Lua["VectorZero"] = FVector::ZeroVector;
	Lua["VectorOne"] = FVector::OneVector;
	Lua["VectorUp"] = FVector::UpVector;
	Lua["VectorRight"] = FVector::RightVector;
	Lua["VectorForward"] = FVector::ForwardVector;
}

void RegisterFRotatorBinding(sol::state& Lua)
{
	Lua.new_usertype<FRotator>(
		"FRotator",
		sol::constructors<FRotator(), FRotator(float, float, float)>(),

		"pitch",
		sol::property(
			[](const FRotator& R) { return R.Pitch; }
		),

		"yaw",
		sol::property(
			[](const FRotator& R) { return R.Yaw; }
		),

		"roll",
		sol::property(
			[](const FRotator& R) { return R.Roll; }
		),

		"Pitch", &FRotator::Pitch,
		"Yaw", &FRotator::Yaw,
		"Roll", &FRotator::Roll
	);
}

// Lua에 GameObject를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterGameObjectBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaGameObjectHandle>(
		"GameObject",

		sol::no_constructor,

		"IsValid",
		[](const FLuaGameObjectHandle& Handle)
		{
			return Handle.IsValid();
		},

		"IsInvalid",
		[](const FLuaGameObjectHandle& Handle)
		{
			return !Handle.IsValid();
		},

		"UUID",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				return Self.UUID;
			}
		),

		"Location",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				AActor* Actor = Self.Resolve();
				return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
			},
			[](const FLuaGameObjectHandle& Self, const FVector& Location)
			{
				AActor* Actor = Self.Resolve();

				if (!Actor)
				{
					UE_LOG("[Lua] Invalid GameObject.Location Access.");
					return;
				}

				Actor->SetActorLocation(Location);
			}
		),

		"Shape",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaShapeComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				UShapeComponent* Shape = FindComponent<UShapeComponent>(Actor);

				if (Shape)
				{
					Handle.UUID = Shape->GetUUID();
				}

				return Handle;
			}
		),

		"ProjectileMovement",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaProjectileMovementComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				UProjectileMovementComponent* Movement =
				FindComponent<UProjectileMovementComponent>(Actor);

				if (Movement)
				{
					Handle.UUID = Movement->GetUUID();
				}

				return Handle;
			}
		),

		"InterpMovement",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaInterpToMoveComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				UInterpToMovementComponent* Movement =
				FindComponent<UInterpToMovementComponent>(Actor);

				if (Movement)
				{
					Handle.UUID = Movement->GetUUID();
				}

				return Handle;
			}
		),

		"PendulumMovement",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaPendulumMovementComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				UPendulumMovementComponent* Movement =
				FindComponent<UPendulumMovementComponent>(Actor);

				if (Movement)
				{
					Handle.UUID = Movement->GetUUID();
				}

				return Handle;
			}
		),

		"RotatingMovement",
		sol::property(
			[](const FLuaGameObjectHandle& Self)
			{
				FLuaRotatingMovementComponentHandle Handle;

				AActor* Actor = Self.Resolve();
				URotatingMovementComponent* Movement =
				FindComponent<URotatingMovementComponent>(Actor);

				if (Movement)
				{
					Handle.UUID = Movement->GetUUID();
				}

				return Handle;
			}
		),

		"PrintLocation",
		[](const FLuaGameObjectHandle& Self)
		{
			AActor* Actor = Self.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] Invalid GameObject.PrintLocation Access.");
				return;
			}

			const FVector Location = Actor->GetActorLocation();

			UE_LOG(
				"[Lua] GameObject UUID=%u, Location=(%.3f, %.3f, %.3f)",
				Actor->GetUUID(),
				Location.X,
				Location.Y,
				Location.Z
			);
		}
	);

	Lua.set_function(
		"FindGameObjectByUUID",
		[](uint32 UUID, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);

			UObject* Object = UObjectManager::Get().FindByUUID(UUID);
			AActor* Actor = Cast<AActor>(Object);

			if (!Actor)
			{
				return sol::nil;
			}

			FLuaGameObjectHandle Handle;
			Handle.UUID = Actor->GetUUID();

			return sol::make_object(LuaView, Handle);
		}
	);
}

void RegisterMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaMovementComponentHandle>(
		"MovementComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaMovementComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaMovementComponentHandle& Self) { return Self.UUID; }),

		"HasValidUpdatedComponent",
		sol::property(
			[](const FLuaMovementComponentHandle& Self)
			{
				UMovementComponent* Movement = Self.Resolve();
				return Movement && Movement->HasValidUpdatedComponent();
			}
		)
	);
}

void RegisterProjectileMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaProjectileMovementComponentHandle>(
		"ProjectileMovementComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaProjectileMovementComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaProjectileMovementComponentHandle& Self) { return Self.UUID; }),

		"Velocity",
		sol::property(
			[](const FLuaProjectileMovementComponentHandle& Self)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetVelocity() : FVector::ZeroVector;
			},
			[](const FLuaProjectileMovementComponentHandle& Self, const FVector& Velocity)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid ProjectileMovementComponent.Velocity Access.");
					return;
				}
				Movement->SetVelocity(Velocity);
			}
		),

		"InitialSpeed",
		sol::property(
			[](const FLuaProjectileMovementComponentHandle& Self)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetInitialSpeed() : 0.0f;
			},
			[](const FLuaProjectileMovementComponentHandle& Self, float InitialSpeed)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid ProjectileMovementComponent.InitialSpeed Access.");
					return;
				}
				Movement->SetInitialSpeed(InitialSpeed);
			}
		),

		"MaxSpeed",
		sol::property(
			[](const FLuaProjectileMovementComponentHandle& Self)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetMaxSpeed() : 0.0f;
			},
			[](const FLuaProjectileMovementComponentHandle& Self, float MaxSpeed)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid ProjectileMovementComponent.MaxSpeed Access.");
					return;
				}
				Movement->SetMaxSpeed(MaxSpeed);
			}
		),

		"PreviewVelocity",
		sol::property(
			[](const FLuaProjectileMovementComponentHandle& Self)
			{
				UProjectileMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetPreviewVelocity() : FVector::ZeroVector;
			}
		),

		"StopSimulating",
		[](const FLuaProjectileMovementComponentHandle& Self)
		{
			UProjectileMovementComponent* Movement = Self.Resolve();
			if (!Movement)
			{
				UE_LOG("[Lua] Invalid ProjectileMovementComponent.StopSimulating Access.");
				return;
			}
			return Movement->StopSimulating();
		}
	);
}
void RegisterInterpToMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaInterpToMoveComponentHandle>(
		"InterpToMoveComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaInterpToMoveComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaInterpToMoveComponentHandle& Self) { return Self.UUID; }),

		"Duration",
		sol::property(
			[](const FLuaInterpToMoveComponentHandle& Self)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetInterpDuration() : 0.0f;
			},
			[](const FLuaInterpToMoveComponentHandle& Self, float Duration)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid InterpToMoveComponent.Duration Access.");
					return;
				}
				Movement->SetInterpDuration(Duration);
			}
		),

		"AutoActivate",
		sol::property(
			[](const FLuaInterpToMoveComponentHandle& Self)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->IsAutoActivating() : false;
			},
			[](const FLuaInterpToMoveComponentHandle& Self, bool AutoActivate)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid InterpToMoveComponent.AutoActivate Access.");
					return;
				}
				Movement->ShouldAutoActivate(AutoActivate);
			}
		),

		"FaceTargetDir",
		sol::property(
			[](const FLuaInterpToMoveComponentHandle& Self)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->IsFacingTargetDir() : false;
			},
			[](const FLuaInterpToMoveComponentHandle& Self, bool FaceTargetDir)
			{
				UInterpToMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid InterpToMoveComponent.FaceTargetDir Access.");
					return;
				}
				Movement->ShouldFaceTargetDir(FaceTargetDir);
			}
		),

		"Initiate",
		[](const FLuaInterpToMoveComponentHandle& Self)
		{
			UInterpToMovementComponent* Movement = Self.Resolve();
			if (!Movement)
			{
				UE_LOG("[Lua] Invalid InterpToMoveComponent.Initiate Access.");
				return;
			}
			Movement->Initiate();
		},

		"Reset",
		[](const FLuaInterpToMoveComponentHandle& Self)
		{
			UInterpToMovementComponent* Movement = Self.Resolve();
			if (!Movement)
			{
				UE_LOG("[Lua] Invalid InterpToMoveComponent.Reset Access.");
				return;
			}
			Movement->Reset();
		},

		"ResetAndHalt",
		[](const FLuaInterpToMoveComponentHandle& Self)
		{
			UInterpToMovementComponent* Movement = Self.Resolve();
			if (!Movement)
			{
				UE_LOG("[Lua] Invalid InterpToMoveComponent.ResetAndHalt Access.");
				return;
			}
			Movement->ResetAndHalt();
		}
	);
}
void RegisterPendulumMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaPendulumMovementComponentHandle>(
		"PendulumMovementComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaPendulumMovementComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaPendulumMovementComponentHandle& Self) { return Self.UUID; }),

		"Axis",
		sol::property(
			[](const FLuaPendulumMovementComponentHandle& Self)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetAxis() : FVector::UpVector;
			},
			[](const FLuaPendulumMovementComponentHandle& Self, const FVector& Axis)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid PendulumMovementComponent.Axis Access.");
					return;
				}
				Movement->SetAxis(Axis);
			}
		),

		"Amplitude",
		sol::property(
			[](const FLuaPendulumMovementComponentHandle& Self)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetAmplitude() : 0.0f;
			},
			[](const FLuaPendulumMovementComponentHandle& Self, float Amplitude)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid PendulumMovementComponent.Amplitude Access.");
					return;
				}
				Movement->SetAmplitude(Amplitude);
			}
		),

		"Frequency",
		sol::property(
			[](const FLuaPendulumMovementComponentHandle& Self)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetFrequency() : 0.0f;
			},
			[](const FLuaPendulumMovementComponentHandle& Self, float Frequency)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid PendulumMovementComponent.Frequency Access.");
					return;
				}
				Movement->SetFrequency(Frequency);
			}
		),

		"Phase",
		sol::property(
			[](const FLuaPendulumMovementComponentHandle& Self)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetPhase() : 0.0f;
			},
			[](const FLuaPendulumMovementComponentHandle& Self, float Phase)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid PendulumMovementComponent.Phase Access.");
					return;
				}
				Movement->SetPhase(Phase);
			}
		),

		"AngleOffset",
		sol::property(
			[](const FLuaPendulumMovementComponentHandle& Self)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetAngleOffset() : 0.0f;
			},
			[](const FLuaPendulumMovementComponentHandle& Self, float AngleOffset)
			{
				UPendulumMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid PendulumMovementComponent.AngleOffset Access.");
					return;
				}
				Movement->SetAngleOffset(AngleOffset);
			}
		)
	);
}
void RegisterRotatingMovementComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaRotatingMovementComponentHandle>(
		"RotatingMovementComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaRotatingMovementComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaRotatingMovementComponentHandle& Self) { return Self.UUID; }),

		"RotationRate",
		sol::property(
			[](const FLuaRotatingMovementComponentHandle& Self)
			{
				URotatingMovementComponent* Movement = Self.Resolve();

				if (!Movement)
				{
					return FRotator::ZeroRotator;
				}
				return Movement->GetRotationRate();
			},
			[](const FLuaRotatingMovementComponentHandle& Self, const FRotator& RotationRate)
			{
				URotatingMovementComponent* Movement = Self.Resolve();

				if (!Movement)
				{
					UE_LOG("[Lua] Invalid RotatingMovementComponent.RotationRate Access.");
					return;
				}

				Movement->SetRotationRate(RotationRate);
			}
		),

		"RotationInLocalSpace",
		sol::property(
			[](const FLuaRotatingMovementComponentHandle& Self)
			{
				URotatingMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->IsRotationInLocalSpace() : false;
			},
			[](const FLuaRotatingMovementComponentHandle& Self, bool RotationInLocalSpace)
			{
				URotatingMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid RotatingMovementComponent.RotationInLocalSpace Access.");
					return;
				}
				Movement->SetRotationInLocalSpace(RotationInLocalSpace);
			}
		),

		"PivotTranslation",
		sol::property(
			[](const FLuaRotatingMovementComponentHandle& Self)
			{
				URotatingMovementComponent* Movement = Self.Resolve();
				return Movement ? Movement->GetPivotTranslation() : FVector::ZeroVector;
			},
			[](const FLuaRotatingMovementComponentHandle& Self, const FVector& PivotTranslation)
			{
				URotatingMovementComponent* Movement = Self.Resolve();
				if (!Movement)
				{
					UE_LOG("[Lua] Invalid RotatingMovementComponent.PivotTranslation Access.");
					return;
				}
				Movement->SetPivotTranslation(PivotTranslation);
			}
		),

		"ResetWorldPivotCache",
		[](const FLuaRotatingMovementComponentHandle& Self)
		{
			URotatingMovementComponent* Movement = Self.Resolve();
			if (!Movement)
			{
				UE_LOG("[Lua] Invalid RotatingMovementComponent.ResetWorldPivotCache Access.");
				return;
			}
			Movement->ResetWorldPivotCache();
		}
	);
}

// Lua에 충돌용 Shape를 등록
// Handle을 통해 UUID로만 접근 가능
void RegisterShapeComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaShapeComponentHandle>(
		"ShapeComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaShapeComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaShapeComponentHandle& Self) { return Self.UUID; }),

		"ShapeType",
		sol::property(
			[](const FLuaShapeComponentHandle& Self)
			{
				UShapeComponent* Shape = Self.Resolve();

				if (!Shape)
				{
					return static_cast<int>(ECollisionShapeType::None);
				}

				return static_cast<int>(Shape->GetCollisionShapeType());
			}
		),

		"AsBox",
		[](const FLuaShapeComponentHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);

			UShapeComponent* Shape = Self.Resolve();
			UBoxComponent* Box = Cast<UBoxComponent>(Shape);

			if (!Box)
			{
				return sol::nil;
			}

			FLuaBoxComponentHandle Handle;
			Handle.UUID = Box->GetUUID();

			return sol::make_object(LuaView, Handle);
		},

		"AsSphere",
		[](const FLuaShapeComponentHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);

			UShapeComponent* Shape = Self.Resolve();
			USphereComponent* Sphere = Cast<USphereComponent>(Shape);

			if (!Sphere)
			{
				return sol::nil;
			}

			FLuaSphereComponentHandle Handle;
			Handle.UUID = Sphere->GetUUID();

			return sol::make_object(LuaView, Handle);
		},

		"AsCapsule",
		[](const FLuaShapeComponentHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);

			UShapeComponent* Shape = Self.Resolve();
			UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape);

			if (!Capsule)
			{
				return sol::nil;
			}

			FLuaCapsuleComponentHandle Handle;
			Handle.UUID = Capsule->GetUUID();

			return sol::make_object(LuaView, Handle);
		}
	);

	Lua["ShapeType_None"] = static_cast<int>(ECollisionShapeType::None);
	Lua["ShapeType_Box"] = static_cast<int>(ECollisionShapeType::Box);
	Lua["ShapeType_Sphere"] = static_cast<int>(ECollisionShapeType::Sphere);
	Lua["ShapeType_Capsule"] = static_cast<int>(ECollisionShapeType::Capsule);
}

void RegisterBoxComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaBoxComponentHandle>(
		"BoxComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaBoxComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaBoxComponentHandle& Self) { return Self.UUID; }),

		"Extent",
		sol::property(
			[](const FLuaBoxComponentHandle& Self)
			{
				UBoxComponent* Box = Self.Resolve();
				return Box ? Box->GetBoxExtent() : FVector::ZeroVector;
			},
			[](const FLuaBoxComponentHandle& Self, const FVector& Extent)
			{
				UBoxComponent* Box = Self.Resolve();
				if (!Box)
				{
					UE_LOG("[Lua] Invalid BoxComponent.Extent Access.");
					return;
				}

				Box->SetBoxExtent(Extent);
			}
		)
	);
}
void RegisterSphereComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaSphereComponentHandle>(
		"SphereComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaSphereComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaSphereComponentHandle& Self) { return Self.UUID; }),

		"Radius",
		sol::property(
			[](const FLuaSphereComponentHandle& Self)
			{
				USphereComponent* Shape = Self.Resolve();
				return Shape ? Shape->GetSphereRadius() : 0.0f;
			},
			[](const FLuaSphereComponentHandle& Self, float Radius)
			{
				USphereComponent* Shape = Self.Resolve();
				if (!Shape)
				{
					UE_LOG("[Lua] Invalid SphereComponent.Radius Access.");
					return;
				}
				Shape->SetSphereRadius(Radius);
			}
		)
	);
}
void RegisterCapsuleComponentBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaCapsuleComponentHandle>(
		"CapsuleComponent",

		sol::no_constructor,

		"IsValid",
		[](const FLuaCapsuleComponentHandle& Handle) { return Handle.IsValid(); },

		"UUID",
		sol::property([](const FLuaCapsuleComponentHandle& Self) { return Self.UUID; }),

		"Radius",
		sol::property(
			[](const FLuaCapsuleComponentHandle& Self)
			{
				UCapsuleComponent* Capsule = Self.Resolve();
				return Capsule ? Capsule->GetCapsuleRadius() : 0.0f;
			},
			[](const FLuaCapsuleComponentHandle& Self, float Radius)
			{
				UCapsuleComponent* Capsule = Self.Resolve();
				if (!Capsule)
				{
					UE_LOG("[Lua] Invalid CapsuleComponent.Radius Access.");
					return;
				}
				Capsule->SetCapsuleRadius(Radius);
			}
		),

		"HalfHeight",
		sol::property(
			[](const FLuaCapsuleComponentHandle& Self)
			{
				UCapsuleComponent* Capsule = Self.Resolve();
				return Capsule ? Capsule->GetCapsuleHalfHeight() : 0.0f;
			},
			[](const FLuaCapsuleComponentHandle& Self, float HalfHeight)
			{
				UCapsuleComponent* Capsule = Self.Resolve();
				if (!Capsule)
				{
					UE_LOG("[Lua] Invalid CapsuleComponent.HalfHeight Access.");
					return;
				}
				Capsule->SetCapsuleHalfHeight(HalfHeight);
			}
		)
	);
}

// Lua에 Delegate 함수를 등록
void RegisterDelegateBinding(sol::state& Lua)
{
	Lua.set_function(
		"RegisterOnTick",
		[](const FLuaGameObjectHandle& Object, sol::protected_function Function)
		{
			AActor* Actor = Object.Resolve();

			if (!Actor)
			{
				UE_LOG("[Lua] RegisterOnTick Failed: Invalid GameObject.");
				return false;
			}

			// TODO : Delegate 함수 등록
			// 예시: 
			// FLuaDelegateManager::Get().RegisterActorTick(Actor->GetUUID(), Function);

			UE_LOG("[Lua] RegisterOnTick called for Actor UUID = %u", Actor->GetUUID());
			return true;
		}
	);
}
