#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Component/ControllerInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Input/InputSystem.h"
#include "Object/Object.h"

#include <windows.h>

namespace
{
	FRotator GetCameraWorldRotation(const UCameraComponent* Camera)
	{
		return Camera ? Camera->GetWorldMatrix().ToRotator() : FRotator();
	}

	void SyncControllerRotationFromCamera(APlayerController* Controller, UCameraComponent* Camera)
	{
		if (!Controller || !Camera)
		{
			return;
		}
		FRotator Rotation = GetCameraWorldRotation(Camera);
		Rotation.Roll = 0.0f;
		Controller->SetControlRotation(Rotation);
	}
}

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::OnBeginPIE(UCameraComponent* InitialTarget, FViewport* InViewport)
{
	Viewport = InViewport;
	Possess(InitialTarget);
	ResetInputState();
}

void UGameViewportClient::OnEndPIE()
{
	SetPossessed(false);
	SetPlayerController(nullptr);
	UnPossess();
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;
}

bool UGameViewportClient::ProcessPIEInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	return Tick(DeltaTime, Snapshot);
}

void UGameViewportClient::SetPIEPossessedInputEnabled(bool bEnabled)
{
	SetPossessed(bEnabled);
}

void UGameViewportClient::SetCursorClipRect(const FRect& InViewportScreenRect)
{
	if (InViewportScreenRect.Width <= 1.0f || InViewportScreenRect.Height <= 1.0f)
	{
		bHasCursorClipRect = false;
		if (bCursorCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	CursorClipClientRect.left = static_cast<LONG>(InViewportScreenRect.X);
	CursorClipClientRect.top = static_cast<LONG>(InViewportScreenRect.Y);
	CursorClipClientRect.right = static_cast<LONG>(InViewportScreenRect.X + InViewportScreenRect.Width);
	CursorClipClientRect.bottom = static_cast<LONG>(InViewportScreenRect.Y + InViewportScreenRect.Height);
	bHasCursorClipRect = CursorClipClientRect.right > CursorClipClientRect.left
		&& CursorClipClientRect.bottom > CursorClipClientRect.top;

	if (bCursorCaptured)
	{
		ApplyCursorClip();
	}
}

void UGameViewportClient::SetPossessed(bool bPossessed)
{
	if (bPIEPossessedInputEnabled == bPossessed)
	{
		return;
	}
	bPIEPossessedInputEnabled = bPossessed;
	SetCursorCaptured(bPossessed);
	ResetInputState();
}

void UGameViewportClient::SetPlayerController(APlayerController* InController)
{
	if (PlayerController == InController)
	{
		return;
	}
	PlayerController = InController;
	SyncControllerRotationFromCamera(PlayerController, GetPossessedTarget());
	ResetInputState();
}

void UGameViewportClient::Possess(UCameraComponent* TargetCamera)
{
	if (PossessedCamera == TargetCamera)
	{
		return;
	}
	PossessedCamera = TargetCamera;
	SyncControllerRotationFromCamera(PlayerController, GetPossessedTarget());
	ResetInputState();
}

void UGameViewportClient::UnPossess()
{
	PossessedCamera = nullptr;
	SetCursorCaptured(false);
	InputSystem::Get().SetUseRawMouse(false);
	ResetInputState();
}

UCameraComponent* UGameViewportClient::GetPossessedTarget() const
{
	if (PlayerController && IsAliveObject(PlayerController))
	{
		if (UCameraComponent* Camera = PlayerController->ResolveViewCamera())
		{
			return Camera;
		}
	}
	return IsAliveObject(PossessedCamera) ? PossessedCamera : nullptr;
}

void UGameViewportClient::ResetInputState()
{
	InputSystem::Get().ResetMouseDelta();
	InputSystem::Get().ResetWheelDelta();
}

bool UGameViewportClient::Tick(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!bPIEPossessedInputEnabled || !HasPossessedTarget())
	{
		return false;
	}

	if (!Snapshot.bWindowFocused)
	{
		InputSystem::Get().SetUseRawMouse(false);
		SetCursorCaptured(false);
		ResetInputState();
		return false;
	}

	InputSystem::Get().SetUseRawMouse(true);
	SetCursorCaptured(true);

	if (!bNativeControllerInputEnabled)
	{
		return true;
	}

	return ApplyInputToCameraOrActor(DeltaTime, Snapshot);
}

bool UGameViewportClient::ApplyInputToCameraOrActor(float DeltaTime, const FInputSystemSnapshot& Snapshot)
{
	if (!HasPossessedTarget())
	{
		return false;
	}
	APlayerController* SafeController = IsAliveObject(PlayerController) ? PlayerController : nullptr;
	if (SafeController)
	{
		if (UControllerInputComponent* InputComponent = SafeController->FindControllerInputComponent())
		{
			return InputComponent->ApplyInput(SafeController, PossessedCamera, DeltaTime, Snapshot);
		}
	}
	return false;
}

void UGameViewportClient::SetCursorCaptured(bool bCaptured)
{
	if (bCursorCaptured == bCaptured)
	{
		if (bCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}
	bCursorCaptured = bCaptured;
	if (bCursorCaptured)
	{
		while (::ShowCursor(FALSE) >= 0) {}
		ApplyCursorClip();
		return;
	}
	while (::ShowCursor(TRUE) < 0) {}
	::ClipCursor(nullptr);
}

void UGameViewportClient::ApplyCursorClip()
{
	if (!OwnerHWnd)
	{
		return;
	}
	RECT ClientRect = {};
	if (bHasCursorClipRect)
	{
		ClientRect = CursorClipClientRect;
	}
	else if (!::GetClientRect(OwnerHWnd, &ClientRect))
	{
		return;
	}
	POINT TopLeft = { ClientRect.left, ClientRect.top };
	POINT BottomRight = { ClientRect.right, ClientRect.bottom };
	if (!::ClientToScreen(OwnerHWnd, &TopLeft) || !::ClientToScreen(OwnerHWnd, &BottomRight))
	{
		return;
	}
	RECT ScreenRect = { TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y };
	if (ScreenRect.right > ScreenRect.left && ScreenRect.bottom > ScreenRect.top)
	{
		::ClipCursor(&ScreenRect);
	}
}
