#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"

struct ISparseSet
{
public:
	virtual ~ISparseSet() = default;
	virtual void Remove(U32 entity) = 0;
	virtual bool Has(U32 entity) const = 0;
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

private:
	Vector<T> denseData;
	Vector<U32> denseEntities;
	Vector<U32> sparseIndices;
};