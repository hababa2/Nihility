#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"
#include "Core/DataReader.hpp"
#include "Core/DataWriter.hpp"

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

struct ISparseSet
{
public:
	virtual ~ISparseSet() = default;
	virtual void Remove(U32 entity) = 0;
	virtual bool Has(U32 entity) const = 0;

	virtual void Serialize(U32 entity, DataWriter& writer) = 0;
	virtual void Deserialize(U32 entity, DataReader& reader) = 0;
	virtual U64 GetTypeHash() const = 0;
};

template<typename T>
struct SparseSet : public ISparseSet
{
public:
	template<typename... Args>
	T& Add(U32 entity, Args&&... args)
	{
		if (entity >= sparseIndices.size())
		{
			sparseIndices.resize(entity + 1, U32_MAX);
		}
		else if (Has(entity))
		{
			T& existing = Get(entity);
			existing = T(Forward<Args>(args)...);
			return existing;
		}

		sparseIndices[entity] = (U32)denseData.size();
		denseEntities.push_back(entity);
		denseData.emplace_back(Forward<Args>(args)...);

		return denseData.back();
	}

	T& Get(U32 entity)
	{
		return denseData[sparseIndices[entity]];
	}

	bool Has(U32 entity) const final
	{
		return entity < sparseIndices.size() && sparseIndices[entity] != U32_MAX;
	}

	std::span<T> GetDenseData() { return denseData; }
	std::span<const U32> GetDenseEntities() const { return denseEntities; }

	void Remove(U32 entity) final
	{
		U32 denseIndex = sparseIndices[entity];
		U32 lastDenseIndex = (U32)denseData.size() - 1;

		if (denseIndex != lastDenseIndex)
		{
			denseData[denseIndex] = Move(denseData[lastDenseIndex]);

			U32 lastEntity = denseEntities[lastDenseIndex];
			denseEntities[denseIndex] = lastEntity;
			sparseIndices[lastEntity] = denseIndex;
		}

		denseData.pop_back();
		denseEntities.pop_back();

		sparseIndices[entity] = U32_MAX;
	}

	void Serialize(U32 entity, DataWriter& writer) override
	{
		T& t = Get(entity);
		t.Serialize(writer);
	}

	void Deserialize(U32 entity, DataReader& reader) override
	{
		T t{};
		t.Deserialize(reader);
		Add(entity, t);
	}

	U64 GetTypeHash() const override
	{
		return GetComponentHash<T>();
	}

private:
	Vector<T> denseData;
	Vector<U32> denseEntities;
	Vector<U32> sparseIndices;
};