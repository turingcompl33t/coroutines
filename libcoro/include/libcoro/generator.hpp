///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

// generator.hpp
// A lazy, potentially infinite sequence of values.
//
// Adapted / simplified from the implementation in CppCoro:
// https://github.com/lewissbaker/cppcoro

#ifndef CORO_GENERATOR_HPP
#define CORO_GENERATOR_HPP

#include <utility>
#include <iterator>
#include <exception>
#include <functional>
#include <type_traits>

#include <stdcoro/coroutine.hpp>

namespace coro
{
	template<typename T>
	class generator;

	namespace detail
	{
		template<typename T>
		class generator_promise
		{
		public:

			using value_type     = std::remove_reference_t<T>;
			using reference_type = std::conditional_t<std::is_reference_v<T>, T, T&>;
			using pointer_type   = value_type*;

			generator_promise() = default;

			generator<T> get_return_object() noexcept;

			constexpr auto initial_suspend() const 
            { 
                return stdcoro::suspend_always{}; 
            }

			constexpr auto final_suspend() const 
            { 
                return stdcoro::suspend_always{}; 
            }

			template<
				typename U = T,
				std::enable_if_t<!std::is_rvalue_reference<U>::value, int> = 0>
			auto yield_value(std::remove_reference_t<T>& value) noexcept
			{
				m_value = std::addressof(value);
				return stdcoro::suspend_always{};
			}

			auto yield_value(std::remove_reference_t<T>&& value) noexcept
			{
				m_value = std::addressof(value);
				return stdcoro::suspend_always{};
			}

			void unhandled_exception()
			{
				m_exception = std::current_exception();
			}

			void return_void() {}

			reference_type value() const noexcept
			{
				return static_cast<reference_type>(*m_value);
			}

			// Don't allow any use of 'co_await' inside the generator coroutine.
			template<typename U>
			stdcoro::suspend_never await_transform(U&& value) = delete;

			void rethrow_if_exception()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:
			pointer_type       m_value;
			std::exception_ptr m_exception;

		};

        struct generator_sentinel {};

		template<typename T>
		class generator_iterator
		{
			using coroutine_handle = stdcoro::coroutine_handle<generator_promise<T>>;

		public:

			using iterator_category = std::input_iterator_tag;
			
			using difference_type = std::ptrdiff_t;
			using value_type      = typename generator_promise<T>::value_type;
			using reference       = typename generator_promise<T>::reference_type;
			using pointer         = typename generator_promise<T>::pointer_type;

			// Iterator needs to be default-constructible to satisfy the Range concept.
			generator_iterator() noexcept
				: m_coroutine(nullptr)
			{}
			
			explicit generator_iterator(coroutine_handle coroutine) noexcept
				: m_coroutine(coroutine)
			{}

			friend bool operator==(const generator_iterator& it, generator_sentinel) noexcept
			{
				return !it.m_coroutine || it.m_coroutine.done();
			}

			friend bool operator!=(const generator_iterator& it, generator_sentinel s) noexcept
			{
				return !(it == s);
			}

			friend bool operator==(generator_sentinel s, const generator_iterator& it) noexcept
			{
				return (it == s);
			}

			friend bool operator!=(generator_sentinel s, const generator_iterator& it) noexcept
			{
				return it != s;
			}

			generator_iterator& operator++()
			{
				m_coroutine.resume();
				if (m_coroutine.done())
				{
					m_coroutine.promise().rethrow_if_exception();
				}

				return *this;
			}

			void operator++(int)
			{
				(void)operator++();
			}

			reference operator*() const noexcept
			{
				return m_coroutine.promise().value();
			}

			pointer operator->() const noexcept
			{
				return std::addressof(operator*());
			}

		private:
			coroutine_handle m_coroutine;
		};

	}  // detail 

	template<typename T>
	class [[nodiscard]] generator
	{
	public:
		using promise_type = detail::generator_promise<T>;
		using iterator     = detail::generator_iterator<T>;

		generator() noexcept
			: m_coroutine{nullptr} {}

		~generator()
		{
			if (m_coroutine)
			{
				m_coroutine.destroy();
			}
		}

		generator(generator const& other)      = delete;
        generator& operator=(generator const&) = delete;

		generator(generator&& other) noexcept
			: m_coroutine{other.m_coroutine}
		{
			other.m_coroutine = nullptr;
		}

		iterator begin()
		{
			if (m_coroutine)
			{
				m_coroutine.resume();
				if (m_coroutine.done())
				{
					m_coroutine.promise().rethrow_if_exception();
				}
			}

			return iterator{ m_coroutine };
		}

		auto end() noexcept
		{
			return detail::generator_sentinel{};
		}

		void swap(generator& other) noexcept
		{
			std::swap(m_coroutine, other.m_coroutine);
		}

	private:

		friend class detail::generator_promise<T>;

		explicit generator(stdcoro::coroutine_handle<promise_type> coroutine) noexcept
			: m_coroutine{coroutine}
		{}

		stdcoro::coroutine_handle<promise_type> m_coroutine;
	};

	template<typename T>
	void swap(generator<T>& a, generator<T>& b)
	{
		a.swap(b);
	}

	namespace detail
	{
		template<typename T>
		generator<T> generator_promise<T>::get_return_object() noexcept
		{
			using coroutine_handle = stdcoro::coroutine_handle<generator_promise<T>>;
			return generator<T>{ coroutine_handle::from_promise(*this) };
		}
	}
}

#endif // CORO_GENERATOR_HPP