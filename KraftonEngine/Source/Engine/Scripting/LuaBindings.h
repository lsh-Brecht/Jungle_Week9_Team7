#pragma once

namespace sol
{
	class state;
}

void RegisterLuaBindings(sol::state& Lua);

void RegisterFVectorBinding(sol::state& Lua);
void RegisterFRotatorBinding(sol::state& Lua);
void RegisterGameObjectBinding(sol::state& Lua);

void RegisterMovementComponentBinding(sol::state& Lua);
void RegisterProjectileMovementComponentBinding(sol::state& Lua);
void RegisterInterpToMovementComponentBinding(sol::state& Lua);
void RegisterPendulumMovementComponentBinding(sol::state& Lua);
void RegisterRotatingMovementComponentBinding(sol::state& Lua);

void RegisterShapeComponentBinding(sol::state& Lua);
void RegisterBoxComponentBinding(sol::state& Lua);
void RegisterSphereComponentBinding(sol::state& Lua);
void RegisterCapsuleComponentBinding(sol::state& Lua);

void RegisterDelegateBinding(sol::state& Lua);
