#pragma once

#include "Core/Log.h"

// Handle 타입에 공통으로 필요한 Lua 멤버를 등록
#define LUA_HANDLE_COMMON(HANDLE_TYPE) \
	"IsValid", \
	[](const HANDLE_TYPE& Handle) \
	{ \
		return Handle.IsValid(); \
	}, \
	"UUID", \
	sol::property( \
		[](const HANDLE_TYPE& Self) \
		{ \
			return Self.UUID; \
		} \
	)

// Component의 읽기 전용 property를 Lua에 등록
#define LUA_COMPONENT_RO_PROPERTY(TYPE_NAME, PROPERTY_NAME, HANDLE_TYPE, COMPONENT_TYPE, VALUE_TYPE, DEFAULT_VALUE, GETTER_CALL) \
	PROPERTY_NAME, \
	sol::property( \
		[](const HANDLE_TYPE& Self) -> VALUE_TYPE \
		{ \
			COMPONENT_TYPE* Component = Self.Resolve(); \
			return Component ? Component->GETTER_CALL : DEFAULT_VALUE; \
		} \
	)

// Component의 읽기/쓰기 property를 Lua에 등록
#define LUA_COMPONENT_RW_PROPERTY(TYPE_NAME, PROPERTY_NAME, HANDLE_TYPE, COMPONENT_TYPE, VALUE_TYPE, DEFAULT_VALUE, GETTER_CALL, SETTER_CALL) \
	PROPERTY_NAME, \
	sol::property( \
		[](const HANDLE_TYPE& Self) -> VALUE_TYPE \
		{ \
			COMPONENT_TYPE* Component = Self.Resolve(); \
			return Component ? Component->GETTER_CALL : DEFAULT_VALUE; \
		}, \
		[](const HANDLE_TYPE& Self, const VALUE_TYPE& Value) \
		{ \
			COMPONENT_TYPE* Component = Self.Resolve(); \
			if (!Component) \
			{ \
				UE_LOG("[Lua] Invalid " TYPE_NAME "." PROPERTY_NAME " Access."); \
				return; \
			} \
			Component->SETTER_CALL; \
		} \
	)

// Component의 멤버 함수를 Lua 메서드로 등록
#define LUA_COMPONENT_METHOD(TYPE_NAME, METHOD_NAME, HANDLE_TYPE, COMPONENT_TYPE, METHOD_CALL) \
	METHOD_NAME, \
	[](const HANDLE_TYPE& Self) \
	{ \
		COMPONENT_TYPE* Component = Self.Resolve(); \
		if (!Component) \
		{ \
			UE_LOG("[Lua] Invalid " TYPE_NAME "." METHOD_NAME " Call."); \
			return; \
		} \
		Component->METHOD_CALL; \
	}

// GameObject에서 특정 컴포넌트를 찾아 Lua Handle로 반환하는 property를 등록
#define LUA_GAMEOBJECT_COMPONENT_PROPERTY(PROPERTY_NAME, HANDLE_TYPE, COMPONENT_TYPE) \
	PROPERTY_NAME, \
	sol::property( \
		[](const FLuaGameObjectHandle& Self) \
		{ \
			HANDLE_TYPE Handle; \
			AActor* Actor = Self.Resolve(); \
			COMPONENT_TYPE* Component = FindComponent<COMPONENT_TYPE>(Actor); \
			if (Component) \
			{ \
				Handle.UUID = Component->GetUUID(); \
			} \
			return Handle; \
		} \
	)

// 부모 Handle에서 자식 타입 Handle로 다운캐스팅하는 Lua 메서드를 등록
#define LUA_HANDLE_DOWNCAST_METHOD(METHOD_NAME, SOURCE_HANDLE_TYPE, SOURCE_COMPONENT_TYPE, TARGET_HANDLE_TYPE, TARGET_COMPONENT_TYPE) \
	METHOD_NAME, \
	[](const SOURCE_HANDLE_TYPE& Self, sol::this_state State) -> sol::object \
	{ \
		sol::state_view LuaView(State); \
		SOURCE_COMPONENT_TYPE* Source = Self.Resolve(); \
		TARGET_COMPONENT_TYPE* Target = Cast<TARGET_COMPONENT_TYPE>(Source); \
		if (!Target) \
		{ \
			return sol::nil; \
		} \
		TARGET_HANDLE_TYPE Handle; \
		Handle.UUID = Target->GetUUID(); \
		return sol::make_object(LuaView, Handle); \
	}

// 일반 getter 호출로 처리하기 어려운 읽기 전용 property를 등록
#define LUA_COMPONENT_RO_PROPERTY_EXPR(TYPE_NAME, PROPERTY_NAME, HANDLE_TYPE, COMPONENT_TYPE, VALUE_TYPE, DEFAULT_VALUE, EXPR) \
	PROPERTY_NAME, \
	sol::property( \
		[](const HANDLE_TYPE& Self) -> VALUE_TYPE \
		{ \
			COMPONENT_TYPE* Component = Self.Resolve(); \
			if (!Component) \
			{ \
				return DEFAULT_VALUE; \
			} \
			return EXPR; \
		} \
	)
