#include "Games/Crossy/Scripting/CrossyLuaBindings.h"

#include "Scripting/SolInclude.h"

void RegisterCrossyLuaBindings(sol::state& Lua)
{
	RegisterCrossyHopMovementComponentBinding(Lua);
	RegisterCrossyParryComponentBinding(Lua);
	RegisterCrossyGameObjectComponentBindings(Lua);
	RegisterCrossyRowManagerBinding(Lua);
	RegisterCrossyUiBinding(Lua);
	RegisterCrossySaveGameBinding(Lua);
}
