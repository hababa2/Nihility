#pragma once

#include "Defines.hpp"
#include "Components.hpp"

#include "Containers/SparseSet.hpp"

#include <tuple>

constexpr U64 HashTypeString(const char* str)
{
	U64 hash = 14695981039346656037ull;
	while (*str)
	{
		hash ^= static_cast<U64>(*str++);
		hash *= 1099511628211ull;
	}
	return hash;
}

template<typename T>
constexpr U64 GetComponentHash()
{
#if defined(_MSC_VER)
	return HashTypeString(__FUNCSIG__);
#else
	return HashTypeString(__PRETTY_FUNCTION__);
#endif
}

struct NH_API Entity
{
public:
	template<class Component, typename... Args>
	Component& AddComponent(Args&&... args);

	template<class Component>
	Component& GetComponent();

	template<class Component>
	void RemoveComponent();

	Transform2D& Transform();

	U32 Id() const { return id; }
	bool Valid() const { return id != U32_MAX; }

private:
	U32 id = U32_MAX;

	friend class Registry;
};

template<typename Lead, typename... Others>
class ComponentView
{
private:
	SparseSet<Lead>& leadSet;
	std::tuple<SparseSet<Others>&...> otherSets;

public:
	ComponentView(SparseSet<Lead>& lead, SparseSet<Others>&... others)
		: leadSet(lead), otherSets(others...)
	{}

	U32 Size() const
	{
		return (U32)leadSet.GetDenseEntities().size();
	}

	U32 GetEntity(U32 denseIndex) const
	{
		return leadSet.GetDenseEntities()[denseIndex];
	}

	bool Matches(U32 id) const
	{
		if constexpr (sizeof...(Others) == 0)
		{
			return true;
		}
		else
		{
			return std::apply([id](auto&... sets) {
				return (sets.Has(id) && ...);
			}, otherSets);
		}
	}

	std::tuple<Lead&, Others&...> Get(U32 id)
	{
		return std::apply([id, this](auto&... sets) {
			return std::forward_as_tuple(leadSet.Get(id), sets.Get(id)...);
		}, otherSets);
	}
};

using ComponentUpdateFn = void(*)();

struct ComponentNode
{
	String name;
	ComponentUpdateFn updateFunc;
	Vector<String> dependencies;
};

class NH_API Registry
{
public:
	static Entity CreateEntity(const Transform2D& transform);
	static Entity CreateEntity(glm::vec2 position = { 0.0f, 0.0f }, glm::vec2 scale = { 1.0f, 1.0f }, F32 rotation = { 0.0f });

	static void DestroyEntity(Entity& entity);

	static Transform2D& GetTransform(U32 id);

	template<class Component, typename... Args>
	static Component& AddComponent(U32 id, Args&&... args);

	template<typename Component>
	static Component& GetComponent(U32 id);

	template<typename Component>
	static bool HasComponent(U32 id);

	template<typename Component>
	static void RemoveComponent(U32 id);

	template<typename Component>
	static SparseSet<Component>& GetSet();

	template<typename Lead, typename... Others>
	static ComponentView<Lead, Others...> View();

	static void RegisterComponentUpdate(const String& name, ComponentUpdateFn func, const Vector<String>& dependencies = {});

private:
	static bool Initialize();
	static void Shutdown();
	static void Update();

	static bool CompileComponentGraph();

	static void* InternalGetOrCreateSet(U64 typeHash, void* (*Allocator)());

	static Vector<Transform2D> transforms;
	static Vector<U32> freeEntities;

	static Vector<ISparseSet*> componentPools;

	static Vector<ComponentNode> registeredComponentUpdates;
	static Vector<ComponentUpdateFn> executionOrder;

	friend struct Entity;
	friend class Nihility;

	STATIC_CLASS(Registry);
};

template<class Component, typename... Args>
inline Component& Entity::AddComponent(Args&&... args)
{
	return Registry::AddComponent<Component>(id, Forward<Args>(args)...);
}

template<class Component>
inline Component& Entity::GetComponent()
{
	return Registry::GetComponent<Component>(id);
}

template<class Component>
inline void Entity::RemoveComponent()
{
	Registry::RemoveComponent<Component>(id);
}

template<class Component, typename... Args>
inline Component& Registry::AddComponent(U32 id, Args&&... args)
{
	return GetSet<Component>().Add(id, Forward<Args>(args)...);
}

template<typename Component>
inline Component& Registry::GetComponent(U32 id)
{
	return GetSet<Component>().Get(id);
}

template<typename Component>
inline bool Registry::HasComponent(U32 id)
{
	return GetSet<Component>().Has(id);
}

template<typename Component>
inline void Registry::RemoveComponent(U32 id)
{
	return GetSet<Component>().Remove(id);
}

template<typename Component>
inline SparseSet<Component>& Registry::GetSet()
{
	constexpr U64 hash = GetComponentHash<Component>();

	auto Allocator = +[]() -> void* {
		return new SparseSet<Component>();
	};

	void* setPtr = InternalGetOrCreateSet(hash, Allocator);

	return *static_cast<SparseSet<Component>*>(setPtr);
}

template<typename Lead, typename... Others>
inline ComponentView<Lead, Others...> Registry::View()
{
	return ComponentView<Lead, Others...>(GetSet<Lead>(), GetSet<Others>()...);
}