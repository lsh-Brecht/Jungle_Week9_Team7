#include "LuaBindings.h"

#define SOL_ALL_SAFETIES_ON 1
#define SOL_LUAJIT 1

// Lua에 함수를 바인딩함
void RegisterLuaBindings(sol::state& Lua)
{
	RegisterFVectorBinding(Lua);
	RegisterFVector4Binding(Lua);
	RegisterFRotatorBinding(Lua);

	RegisterActorLifecycleBinding(Lua);
	
	RegisterShapeComponentBinding(Lua);
	RegisterSphereComponentBinding(Lua);
	RegisterBoxComponentBinding(Lua);
	RegisterCapsuleComponentBinding(Lua);

	RegisterStaticMeshComponentBinding(Lua);

	RegisterMovementComponentBinding(Lua);
	RegisterProjectileMovementComponentBinding(Lua);
	RegisterInterpToMovementComponentBinding(Lua);
	RegisterPendulumMovementComponentBinding(Lua);
	RegisterRotatingMovementComponentBinding(Lua);
	
	RegisterGameObjectBinding(Lua);
	RegisterDelegateBinding(Lua);
}