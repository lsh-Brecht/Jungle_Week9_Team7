#include "LuaBindings.h"
#include "SolInclude.h"
#include "LuaHandles.h"
#include "LuaBindingHelper.h"

#include "Camera/CameraTypes.h"
#include "Component/ControllerInputComponent.h"
#include "Core/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/MathUtils.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

#include <algorithm>
#include <cctype>

namespace
{
	FString NormalizeControllerToken(FString Value)
	{
		Value.erase(
			std::remove_if(Value.begin(), Value.end(), [](unsigned char Ch)
			{
				return Ch == ' ' || Ch == '_' || Ch == '-';
			}),
			Value.end());

		for (char& Ch : Value)
		{
			Ch = static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
		}

		return Value;
	}

	const char* MovementBasisToString(EControllerMovementBasis Basis)
	{
		switch (Basis)
		{
		case EControllerMovementBasis::World:
			return "World";
		case EControllerMovementBasis::ControlRotation:
			return "ControlRotation";
		case EControllerMovementBasis::ViewCamera:
		default:
			return "ViewCamera";
		}
	}

	EControllerMovementBasis MovementBasisFromIndex(int32 Index)
	{
		switch (Index)
		{
		case static_cast<int32>(EControllerMovementBasis::World):
			return EControllerMovementBasis::World;
		case static_cast<int32>(EControllerMovementBasis::ControlRotation):
			return EControllerMovementBasis::ControlRotation;
		case static_cast<int32>(EControllerMovementBasis::ViewCamera):
		default:
			return EControllerMovementBasis::ViewCamera;
		}
	}

	EControllerMovementBasis MovementBasisFromName(const FString& Name)
	{
		const FString Token = NormalizeControllerToken(Name);

		if (Token == "WORLD")
		{
			return EControllerMovementBasis::World;
		}
		if (Token == "CONTROL" || Token == "CONTROLROTATION")
		{
			return EControllerMovementBasis::ControlRotation;
		}
		return EControllerMovementBasis::ViewCamera;
	}

	ECameraModeId CameraModeFromIndex(int32 Index)
	{
		if (Index < static_cast<int32>(ECameraModeId::FirstPerson) ||
			Index > static_cast<int32>(ECameraModeId::Cutscene))
		{
			return ECameraModeId::ThirdPerson;
		}
		return static_cast<ECameraModeId>(Index);
	}

	ECameraModeId CameraModeFromName(const FString& Name)
	{
		const FString Token = NormalizeControllerToken(Name);

		if (Token == "FIRSTPERSON" || Token == "FPS")
		{
			return ECameraModeId::FirstPerson;
		}
		if (Token == "ORTHOGRAPHIC" || Token == "ORTHOGRAPHICFOLLOW" || Token == "ORTHO")
		{
			return ECameraModeId::OrthographicFollow;
		}
		if (Token == "PERSPECTIVEFOLLOW")
		{
			return ECameraModeId::PerspectiveFollow;
		}
		if (Token == "EXTERNAL" || Token == "EXTERNALCAMERA")
		{
			return ECameraModeId::ExternalCamera;
		}
		if (Token == "CUTSCENE")
		{
			return ECameraModeId::Cutscene;
		}
		return ECameraModeId::ThirdPerson;
	}

	FCameraBlendParams MakeBlendParams(float BlendTime)
	{
		FCameraBlendParams Params;
		Params.BlendTime = BlendTime < 0.0f ? 0.0f : BlendTime;
		return Params;
	}

	FLuaGameObjectHandle MakeGameObjectHandle(AActor* Actor)
	{
		FLuaGameObjectHandle Handle;
		if (Actor)
		{
			Handle.UUID = Actor->GetUUID();
		}
		return Handle;
	}

	FLuaPawnHandle MakePawnHandle(APawn* Pawn)
	{
		FLuaPawnHandle Handle;
		if (Pawn)
		{
			Handle.UUID = Pawn->GetUUID();
		}
		return Handle;
	}

	void ApplyControllerLookRotation(APlayerController* Controller, FRotator Rotation)
	{
		if (!Controller)
		{
			return;
		}

		Rotation.Roll = 0.0f;
		Controller->SetControlRotation(Rotation);
	}
}

void RegisterPlayerControllerBinding(sol::state& Lua)
{
	Lua.new_usertype<FLuaControllerInputComponentHandle>(
		"ControllerInputComponent",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaControllerInputComponentHandle),

		"MovementBasis",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> FString
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? MovementBasisToString(Input->GetMovementBasis()) : FString();
			},
			[](const FLuaControllerInputComponentHandle& Self, const FString& Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMovementBasis(MovementBasisFromName(Value));
				}
			}
		),

		"MovementBasisIndex",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> int32
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? static_cast<int32>(Input->GetMovementBasis()) : 0;
			},
			[](const FLuaControllerInputComponentHandle& Self, int32 Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMovementBasis(MovementBasisFromIndex(Value));
				}
			}
		),

		"MoveSpeed",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetMoveSpeed() : 0.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMoveSpeed(Value);
				}
			}
		),

		"SprintMultiplier",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetSprintMultiplier() : 0.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetSprintMultiplier(Value);
				}
			}
		),

		"LookSensitivityX",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetLookSensitivityX() : 0.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetLookSensitivityX(Value);
				}
			}
		),

		"LookSensitivityY",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetLookSensitivityY() : 0.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetLookSensitivityY(Value);
				}
			}
		),

		"LookSensitivity",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetLookSensitivityX() : 0.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetLookSensitivity(Value);
				}
			}
		),

		"MinPitch",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetMinPitch() : -89.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMinPitch(Value);
				}
			}
		),

		"MaxPitch",
		sol::property(
			[](const FLuaControllerInputComponentHandle& Self) -> float
			{
				UControllerInputComponent* Input = Self.Resolve();
				return Input ? Input->GetMaxPitch() : 89.0f;
			},
			[](const FLuaControllerInputComponentHandle& Self, float Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMaxPitch(Value);
				}
			}
		),

		"SetMovementBasis",
		sol::overload(
			[](const FLuaControllerInputComponentHandle& Self, const FString& Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMovementBasis(MovementBasisFromName(Value));
				}
			},
			[](const FLuaControllerInputComponentHandle& Self, int32 Value)
			{
				if (UControllerInputComponent* Input = Self.Resolve())
				{
					Input->SetMovementBasis(MovementBasisFromIndex(Value));
				}
			}
		)
	);

	Lua.new_usertype<FLuaPlayerControllerHandle>(
		"PlayerController",

		sol::no_constructor,

		LUA_HANDLE_COMMON(FLuaPlayerControllerHandle),

		"Possess",
		sol::overload(
			[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle)
			{
				APlayerController* Controller = Self.Resolve();
				if (!Controller)
				{
					UE_LOG("[Lua] Invalid PlayerController.Possess(GameObject) Call.");
					return;
				}
				Controller->Possess(ActorHandle.Resolve());
			},
			[](const FLuaPlayerControllerHandle& Self, const FLuaPawnHandle& PawnHandle)
			{
				APlayerController* Controller = Self.Resolve();
				if (!Controller)
				{
					UE_LOG("[Lua] Invalid PlayerController.Possess(Pawn) Call.");
					return;
				}
				Controller->Possess(PawnHandle.Resolve());
			}
		),

		"UnPossess",
		[](const FLuaPlayerControllerHandle& Self)
		{
			if (APlayerController* Controller = Self.Resolve())
			{
				Controller->UnPossess();
			}
		},

		"GetPossessedActor",
		[](const FLuaPlayerControllerHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);
			APlayerController* Controller = Self.Resolve();
			AActor* Actor = Controller ? Controller->GetPossessedActor() : nullptr;
			return Actor ? sol::make_object(LuaView, MakeGameObjectHandle(Actor)) : sol::nil;
		},

		"GetPossessedPawn",
		[](const FLuaPlayerControllerHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);
			APlayerController* Controller = Self.Resolve();
			APawn* Pawn = Cast<APawn>(Controller ? Controller->GetPossessedActor() : nullptr);
			return Pawn ? sol::make_object(LuaView, MakePawnHandle(Pawn)) : sol::nil;
		},

		"GetViewTarget",
		[](const FLuaPlayerControllerHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);
			APlayerController* Controller = Self.Resolve();
			AActor* Actor = Controller ? Controller->GetViewTarget() : nullptr;
			return Actor ? sol::make_object(LuaView, MakeGameObjectHandle(Actor)) : sol::nil;
		},

		"GetControllerInput",
		[](const FLuaPlayerControllerHandle& Self, sol::this_state State) -> sol::object
		{
			sol::state_view LuaView(State);
			APlayerController* Controller = Self.Resolve();
			UControllerInputComponent* Input = Controller ? Controller->FindControllerInputComponent() : nullptr;
			if (!Input)
			{
				return sol::nil;
			}

			FLuaControllerInputComponentHandle Handle;
			Handle.UUID = Input->GetUUID();
			return sol::make_object(LuaView, Handle);
		},

		"Input",
		sol::property(
			[](const FLuaPlayerControllerHandle& Self, sol::this_state State) -> sol::object
			{
				sol::state_view LuaView(State);
				APlayerController* Controller = Self.Resolve();
				UControllerInputComponent* Input = Controller ? Controller->FindControllerInputComponent() : nullptr;
				if (!Input)
				{
					return sol::nil;
				}

				FLuaControllerInputComponentHandle Handle;
				Handle.UUID = Input->GetUUID();
				return sol::make_object(LuaView, Handle);
			}
		),

		"SetViewTarget",
		sol::overload(
			[](const FLuaPlayerControllerHandle& Self, const FLuaPawnHandle& PawnHandle)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetViewTarget(PawnHandle.Resolve());
				}
			},
			[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetViewTarget(ActorHandle.Resolve());
				}
			}
		),

		"SetViewTargetWithBlend",
		sol::overload(
			[](const FLuaPlayerControllerHandle& Self, const FLuaPawnHandle& PawnHandle, float BlendTime)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetViewTargetWithBlend(PawnHandle.Resolve(), MakeBlendParams(BlendTime));
				}
			},
			[](const FLuaPlayerControllerHandle& Self, const FLuaGameObjectHandle& ActorHandle, float BlendTime)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetViewTargetWithBlend(ActorHandle.Resolve(), MakeBlendParams(BlendTime));
				}
			}
		),

		"SetCameraMode",
		sol::overload(
			[](const FLuaPlayerControllerHandle& Self, const FString& ModeName)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetCameraMode(CameraModeFromName(ModeName), MakeBlendParams(0.0f));
				}
			},
			[](const FLuaPlayerControllerHandle& Self, const FString& ModeName, float BlendTime)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetCameraMode(CameraModeFromName(ModeName), MakeBlendParams(BlendTime));
				}
			},
			[](const FLuaPlayerControllerHandle& Self, int32 ModeIndex)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetCameraMode(CameraModeFromIndex(ModeIndex), MakeBlendParams(0.0f));
				}
			},
			[](const FLuaPlayerControllerHandle& Self, int32 ModeIndex, float BlendTime)
			{
				if (APlayerController* Controller = Self.Resolve())
				{
					Controller->SetCameraMode(CameraModeFromIndex(ModeIndex), MakeBlendParams(BlendTime));
				}
			}
		),

		"GetControlRotation",
		[](const FLuaPlayerControllerHandle& Self) -> FRotator
		{
			APlayerController* Controller = Self.Resolve();
			return Controller ? Controller->GetControlRotation() : FRotator();
		},

		"SetControlRotation",
		[](const FLuaPlayerControllerHandle& Self, const FRotator& Rotation)
		{
			ApplyControllerLookRotation(Self.Resolve(), Rotation);
		},

		"AddYawInput",
		[](const FLuaPlayerControllerHandle& Self, float Value)
		{
			if (APlayerController* Controller = Self.Resolve())
			{
				Controller->AddYawInput(Value);
			}
		},

		"AddPitchInput",
		[](const FLuaPlayerControllerHandle& Self, float Value)
		{
			APlayerController* Controller = Self.Resolve();
			if (!Controller)
			{
				return;
			}

			float MinPitch = -89.0f;
			float MaxPitch = 89.0f;
			if (UControllerInputComponent* Input = Controller->FindControllerInputComponent())
			{
				MinPitch = Input->GetMinPitch();
				MaxPitch = Input->GetMaxPitch();
			}
			Controller->AddPitchInput(Value, MinPitch, MaxPitch);
		},

		"ForwardVector",
		sol::property(
			[](const FLuaPlayerControllerHandle& Self) -> FVector
			{
				APlayerController* Controller = Self.Resolve();
				return Controller ? Controller->GetControlRotation().GetForwardVector() : FVector::ForwardVector;
			}
		),

		"RightVector",
		sol::property(
			[](const FLuaPlayerControllerHandle& Self) -> FVector
			{
				APlayerController* Controller = Self.Resolve();
				return Controller ? Controller->GetControlRotation().GetRightVector() : FVector::RightVector;
			}
		),

		"UpVector",
		sol::property(
			[](const FLuaPlayerControllerHandle& Self) -> FVector
			{
				APlayerController* Controller = Self.Resolve();
				return Controller ? Controller->GetControlRotation().GetUpVector() : FVector::UpVector;
			}
		),

		"AddMovementInput",
		[](const FLuaPlayerControllerHandle& Self, const FVector& Direction, float Scale)
		{
			APlayerController* Controller = Self.Resolve();
			APawn* Pawn = Cast<APawn>(Controller ? Controller->GetPossessedActor() : nullptr);
			if (!Pawn)
			{
				UE_LOG("[Lua] PlayerController.AddMovementInput ignored: possessed actor is not Pawn.");
				return;
			}

			Pawn->AddMovementInput(Direction, Scale);
		}
	);
}
