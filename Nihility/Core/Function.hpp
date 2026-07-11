#pragma once

#include "Defines.hpp"

#include <utility>
#include <functional>

template <typename Signature, U64 InlineSize = 128>
class Function;

template <typename Ret, typename... Args, U64 InlineSize>
class Function<Ret(Args...), InlineSize>
{
	enum class Op { Destroy, Move };

	using InvokeFn = Ret(*)(void*, Args...);
	using ManagerFn = void (*)(void* destState, void* srcState, void* destSbo, void* srcSbo, Op op);

	static void EmptyInvoke(void*, Args...) {}

public:
	Function() = default;
	Function(std::nullptr_t) noexcept {}

	template <typename T> requires (!std::same_as<std::decay_t<T>, Function>)
		Function(T&& callable)
	{
		using Decayed = std::decay_t<T>;

		if constexpr (std::is_empty_v<Decayed> && std::is_trivially_default_constructible_v<Decayed>)
		{
			invoke = &Invoke<Decayed>;
			return;
		}

		invoke = &Invoke<Decayed>;
		manager = &Manage<Decayed>;

		if constexpr (IsInline<Decayed>())
		{
			std::construct_at(reinterpret_cast<Decayed*>(sboBuffer), std::forward<T>(callable));
			state = sboBuffer;
		}
		else
		{
			state = new Decayed(std::forward<T>(callable));
		}
	}

	~Function()
	{
		if (manager)
		{
			manager(nullptr, state, nullptr, nullptr, Op::Destroy);
		}
	}

	Function(const Function&) = delete;
	Function& operator=(const Function&) = delete;

	Function(Function&& other) noexcept : invoke(other.invoke), manager(other.manager)
	{
		if (manager)
		{
			manager(&state, other.state, sboBuffer, nullptr, Op::Move);
			if (!state) { state = sboBuffer; }

			other.invoke = &EmptyInvoke;
			other.manager = nullptr;
			other.state = nullptr;
		}
	}

	Function& operator=(Function&& other) noexcept
	{
		if (this != &other)
		{
			this->~Function();
			new (this) Function(std::move(other));
		}
		return *this;
	}

	Function& operator=(std::nullptr_t) noexcept
	{
		this->~Function();
		invoke = &EmptyInvoke;
		manager = nullptr;
		state = nullptr;

		return *this;
	}

	NH_NODISCARD bool IsValid() const noexcept
	{
		return invoke != &EmptyInvoke;
	}

	NH_NODISCARD explicit operator bool() const noexcept
	{
		return IsValid();
	}

	void operator()(Args... args) const
	{
		invoke(state, std::forward<Args>(args)...);
	}

private:
	template <typename T>
	static constexpr bool IsInline()
	{
		return sizeof(T) <= InlineSize && alignof(T) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<T>;
	}

	template <typename T>
	static void Manage(void* destState, void* srcState, void* destSbo, void* srcSbo, Op op)
	{
		T* src = static_cast<T*>(srcState);
		switch (op)
		{
		case Op::Destroy: {
			if constexpr (IsInline<T>()) { std::destroy_at(src); }
			else { delete src; }
		} break;
		case Op::Move: {
			if constexpr (IsInline<T>())
			{
				std::construct_at(static_cast<T*>(destSbo), std::move(*src));
				std::destroy_at(src);
			}
			else { *static_cast<void**>(destState) = src; }
		} break;
		}
	}

	template <typename T>
	static void Invoke(void* state, Args... args)
	{
		std::invoke(*static_cast<T*>(state), std::forward<Args>(args)...);
	}

	InvokeFn invoke = &EmptyInvoke;
	ManagerFn manager = nullptr;

	void* state = nullptr;

	alignas(std::max_align_t) std::byte sboBuffer[InlineSize];
};