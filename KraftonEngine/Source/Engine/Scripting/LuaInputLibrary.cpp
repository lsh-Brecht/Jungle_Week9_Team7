#include "LuaInputLibrary.h"
#include "SolInclude.h"
#include "Engine/Input/InputSystem.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace
{
	FInputSystemSnapshot GCurrentSnapshot;

	int NameToVK(const std::string& Name)
	{
		if (Name.empty())
		{
			return -1;
		}

		if (Name.size() == 1)
		{
			const char C = static_cast<char>(toupper(static_cast<unsigned char>(Name[0])));
			if (C >= 'A' && C <= 'Z')
			{
				return C;
			}
			if (C >= '0' && C <= '9')
			{
				return C;
			}
		}

		static const std::unordered_map<std::string, int> KeyMap = {
			{ "SPACE", VK_SPACE },
			{ "ENTER", VK_RETURN },
			{ "RETURN", VK_RETURN },
			{ "ESCAPE", VK_ESCAPE },
			{ "ESC", VK_ESCAPE },
			{ "SHIFT", VK_SHIFT },
			{ "LSHIFT", VK_LSHIFT },
			{ "RSHIFT", VK_RSHIFT },
			{ "CTRL", VK_CONTROL },
			{ "CONTROL",VK_CONTROL },
			{ "LCTRL", VK_LCONTROL },
			{ "RCTRL", VK_RCONTROL },
			{ "ALT", VK_MENU },
			{ "LALT", VK_LMENU },
			{ "RALT", VK_RMENU },
			{ "TAB", VK_TAB },
			{ "BACKSPACE", VK_BACK },
			{ "DELETE", VK_DELETE },
			{ "INSERT", VK_INSERT },
			{ "HOME", VK_HOME },
			{ "END", VK_END },
			{ "PAGEUP", VK_PRIOR },
			{ "PAGEDOWN", VK_NEXT },
			{ "LEFT", VK_LEFT },
			{ "RIGHT", VK_RIGHT },
			{ "UP", VK_UP },
			{ "DOWN", VK_DOWN },
			{ "F1", VK_F1 },
			{ "F2", VK_F2 },
			{ "F3", VK_F3 },
			{ "F4", VK_F4 },
			{ "F5", VK_F5 },
			{ "F6", VK_F6 },
			{ "F7", VK_F7 },
			{ "F8", VK_F8 },
			{ "F9", VK_F9 },
			{ "F10", VK_F10 },
			{ "F11", VK_F11 },
			{ "F12", VK_F12 },
			{ "MOUSE1", VK_LBUTTON },
			{ "MOUSE2", VK_RBUTTON },
			{ "MOUSE3", VK_MBUTTON },
		};

		std::string Upper = Name;
		for (char& C : Upper)
		{
			C = static_cast<char>(toupper(static_cast<unsigned char>(C)));
		}

		auto It = KeyMap.find(Upper);
		return (It != KeyMap.end()) ? It->second : -1;
	}
}

void FLuaInputLibrary::SetFrameSnapshot(const FInputSystemSnapshot& Snapshot)
{
	GCurrentSnapshot = Snapshot;
}

// LuaBindings.h의 free function 선언에 대응하는 wrapper
void RegisterInputBinding(sol::state& Lua)
{
	FLuaInputLibrary::RegisterInputBinding(Lua);
}

void FLuaInputLibrary::RegisterInputBinding(sol::state& Lua)
{
	sol::table Input = Lua.create_named_table("Input");

	Input.set_function("GetKey", [](const std::string& KeyName) -> bool
	{
		int VK = NameToVK(KeyName);
		return (VK >= 0 && VK < 256) ? GCurrentSnapshot.KeyDown[VK] : false;
	});

	Input.set_function("GetKeyDown", [](const std::string& KeyName) -> bool
	{
		int VK = NameToVK(KeyName);
		return (VK >= 0 && VK < 256) ? GCurrentSnapshot.KeyPressed[VK] : false;
	});

	Input.set_function("GetKeyUp", [](const std::string& KeyName) -> bool
	{
		int VK = NameToVK(KeyName);
		return (VK >= 0 && VK < 256) ? GCurrentSnapshot.KeyReleased[VK] : false;
	});

	Input.set_function("GetMouseDelta", [](sol::this_state TS) -> sol::table
	{
		sol::state_view Lua(TS);
		sol::table T = Lua.create_table();
		T["x"] = static_cast<float>(GCurrentSnapshot.MouseDeltaX);
		T["y"] = static_cast<float>(GCurrentSnapshot.MouseDeltaY);
		return T;
	});

	Input.set_function("GetMousePosition", [](sol::this_state TS) -> sol::table
	{
		sol::state_view Lua(TS);
		sol::table T = Lua.create_table();
		T["x"] = static_cast<float>(GCurrentSnapshot.MousePos.x);
		T["y"] = static_cast<float>(GCurrentSnapshot.MousePos.y);
		return T;
	});

	Input.set_function("GetScroll", []() -> float
	{
		return static_cast<float>(GCurrentSnapshot.ScrollDelta);
	});

	Input.set_function("IsGuiUsingMouse", []() -> bool
	{
		return GCurrentSnapshot.bGuiUsingMouse;
	});

	Input.set_function("IsGuiUsingKeyboard", []() -> bool
	{
		return GCurrentSnapshot.bGuiUsingKeyboard;
	});

	Input.set_function("IsWindowFocused", []() -> bool
	{
		return GCurrentSnapshot.bWindowFocused;
	});

	Input.set_function("IsMouseCaptured", []() -> bool
	{
		return GCurrentSnapshot.bUsingRawMouse;
	});

	Input.set_function("SetMouseCaptured", [](bool bCapture)
	{
		InputSystem::Get().SetUseRawMouse(bCapture);
	});
}
