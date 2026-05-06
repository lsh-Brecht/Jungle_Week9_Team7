# UVignetteModifier 구현 명세서

본 문서는 `UCameraModifier`를 상속받는 `UVignetteModifier`를 구현하여, 플레이어의 사망이나 피격 등 특정 이벤트 발생 시 Vignette 효과를 전역적이고 안정적으로 제어하기 위한 명세를 정의합니다.

---

## 1. 개요
기존의 Vignette 처리는 Lua에서 특정 카메라 객체의 속성을 직접 수정하는 방식이었습니다. 이는 카메라 전환 시 효과가 유실되거나, 잘못된 카메라 객체를 참조하는 문제를 야기했습니다. `UVignetteModifier`를 도입함으로써 `PlayerCameraManager` 수준에서 효과를 관리하여 이러한 문제를 해결합니다.

---

## 2. 세부 구현 계획

### Step 1: `UVignetteModifier` 클래스 (C++)
- **클래스명**: `UVignetteModifier` (상속: `UCameraModifier`)
- **헤더 위치**: `KraftonEngine/Source/Engine/Camera/CameraVignetteModifier.h`
- **소스 위치**: `KraftonEngine/Source/Engine/Camera/CameraVignetteModifier.cpp`
- **주요 기능**:
    - `StartVignette(Intensity, Color, Duration, Smoothness)`: 효과 시작 및 보간 설정.
    - `StopVignette(Duration)`: 효과 서서히 제거.
    - `ModifyCamera(DeltaTime, InOutView)`: `GetAlpha()`를 사용하여 `InOutView.PostProcess`에 Vignette 파라미터 주입.

### Step 2: `PlayerCameraManager` 연동 (C++)
- **위치**: `KraftonEngine/Source/Engine/Camera/PlayerCameraManager.{h,cpp}`
- **추가 항목**:
    - `UVignetteModifier* VignetteModifier` 멤버 변수.
    - `StartVignette(...)`, `StopVignette(...)` 공개 메서드 추가.
    - `EnsureVignetteModifier()` 내부 헬퍼를 통해 모디파이어 생명주기 관리.

### Step 3: Lua Script 바인딩 (Scripting)
- **위치**: `KraftonEngine/Source/Engine/Scripting/LuaPlayerControllerBindings.cpp`
- **추가 항목**:
    - `PlayerController:StartVignette(Intensity, Color, Duration, Smoothness)`
    - `PlayerController:StopVignette(Duration)`

### Step 4: 실 적용 (Lua)
- **위치**: `KraftonEngine/LuaScripts/Game/Player.lua`
- **변경 사항**:
    - 사망 연출(Phase 1) 시 카메라 속성을 직접 건드리는 대신 `Player.controller:StartVignette(...)`를 호출하도록 수정.

---

## 3. 기대 결과
1. **안정성**: 카메라가 바뀌더라도 `PlayerCameraManager`가 관리하는 모디파이어 체인에 의해 Vignette 효과가 지속됨.
2. **코드 간결화**: 보간 로직이 C++ 엔진 레벨로 이동하여 Lua 코드가 단순해짐.
3. **유연성**: 피격 효과 등 다른 시각적 연출에도 쉽게 재사용 가능.
