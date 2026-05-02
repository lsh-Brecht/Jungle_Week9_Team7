#pragma once

struct FInputSystemSnapshot;

namespace sol { class state; }

class FLuaInputLibrary
{
public:
	static void RegisterInputBinding(sol::state& Lua);
	static void SetFrameSnapshot(const FInputSystemSnapshot& Snapshot);
};
