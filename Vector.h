#ifndef VECTOR_H
#define VECTOR_H

#include <cstddef>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <initializer_list>
#include <limits>
#include <ranges> //for std::ranges::sized_range
#include <algorithm>

template <typename T>
class Vector
{
private:
	T* m_Arr;

	std::size_t m_CurrentSize; //# of elements currently in the Vector
	std::size_t m_Capacity; //total # of elements the allocated array can hold before resize is necessary

	void Reallocate(std::size_t newCapacity)
	{
		//----------------------------------------------------------------------------//

		if (newCapacity < m_CurrentSize)
			throw std::runtime_error("newCapacity cannot be < currentSize");

		//arbitrary number for now, 1 mil
		else if (newCapacity > 1000000)
			return;

		//do nothing
		else if (newCapacity <= m_Capacity)
			return;
	//----------------------------------------------------------------------------//
		
		//temp in case of a throw during the move from m_Arr into temp
		T* temp = reinterpret_cast<T*>(::operator new(newCapacity * sizeof(T)));
		std::size_t constructed = 0;

		try
		{
			if constexpr(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
			{
				for (; constructed < m_CurrentSize; ++constructed)
					new(temp + constructed)T(std::move(m_Arr[constructed]));
			}

			else
			{
				for (; constructed < m_CurrentSize; ++constructed)
					new(temp + constructed)T(m_Arr[constructed]);
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < constructed; ++i)
				temp[i].~T();

			::operator delete(temp);
			throw;
		}

		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		::operator delete(m_Arr);

		m_Arr = temp;
		m_Capacity = newCapacity;
	}

										//big ass helper func for insert_range
//================================================================================================================================//
	template <typename It>
	T* insert_range_impl(std::size_t index, std::size_t count, It first, It last)
	{
		std::size_t newSize = m_CurrentSize + count;
		std::size_t newCapacity = std::max(m_Capacity + (m_Capacity / 2), newSize);
		const bool preferMove = (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>);

		//---------------------------------------------------------------------------------
		//no reallocation needed
		//---------------------------------------------------------------------------------
		//force realloc when shifting would require assignment
		const bool needsShift = (index != m_CurrentSize);
		const bool canAssign = (std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>);

		if (newSize <= m_Capacity && m_Arr != nullptr && (!needsShift || canAssign))
		{
			const std::size_t oldSize = m_CurrentSize;
			std::size_t tail_constructed = 0;

			try
			{
				//if we are constructing at the end of the Vector
				if (index == oldSize)
				{
					//copy from container into end of m_Arr
					for (std::size_t i{ 0 }; first != last; ++first, ++i)
					{
						new(&m_Arr[index + i])T(*first);
						++tail_constructed;
					}

					//commit
					m_CurrentSize = oldSize + count;
					return m_Arr + index;
				}

				//not constructing at the end of the Vector
				else
				{
					std::size_t oldSize = m_CurrentSize;

					//Shift elements to the right to create enough open memory for "count" elements at "index"
					//Shift the entire suffix [index, oldSize) right by count:
					//source = from
					//dest   = to = from + count
					for (std::size_t i{ oldSize }; i > index; --i)
					{
						auto from = i - 1;
						auto to = from + count;

						//[1][2][3][4][5] _ _ _
						// ^ constructed      ^ raw
						if (to >= oldSize)
						{
							//destination is raw storage, placement-new
							if (preferMove)
								new(&m_Arr[to])T(std::move(m_Arr[from]));
							else
								new(&m_Arr[to])T(m_Arr[from]);

							++tail_constructed;
						}
						//[1][2][x][x][x][3][4][5]
						//         ^ gap    ^ some of these were constructed via placement - new
						else
						{
							//destination already constructed, copy assign
							if (preferMove)
								m_Arr[to] = std::move(m_Arr[from]);
							else
								m_Arr[to] = m_Arr[from];
						}
					}


					//insert all container elements into memory
					std::size_t inserted = 0;
					for (; first != last; ++first, ++inserted)
					{
						std::size_t at = index + inserted;

						if (at < oldSize)
						{
							//slot already constructed in memory
							m_Arr[at] = *first;
						}
						else
						{
							//slot is raw storage (non constructed memory)
							new(&m_Arr[at])T(*first);
							++tail_constructed;
						}
					}

					if (inserted != count)
						throw std::runtime_error("inserted != count for some reason");

					//commit
					m_CurrentSize += count;
					return m_Arr + index;
				}
			}
			catch (...)
			{
				m_CurrentSize = oldSize;

				//destroy only the objects newly constructed into raw tail storage, lives in [oldSize, oldSize + tail_constructed)
				for (std::size_t i{ 0 }; i < tail_constructed; ++i)
					m_Arr[oldSize + i].~T();
				throw;
			}
		}

		//---------------------------------------------------------------------------------
		//requires reallocation branch
		//---------------------------------------------------------------------------------
		std::size_t constructed = 0;
		std::size_t inserted = 0;

		//allocate storage
		T* temp = reinterpret_cast<T*>(::operator new(newCapacity * sizeof(T)));

		try
		{
			if (preferMove)
			{
				//move from [0,index)
				for (std::size_t i{ 0 }; i < index; ++i)
				{
					new(temp + i)T(std::move(m_Arr[i]));
					++constructed;
				}
			}

			else
			{
				//copy from [0,index)
				for (std::size_t i{ 0 }; i < index; ++i)
				{
					new(temp + i)T(m_Arr[i]);
					++constructed;
				}
			}

			//insert items from container
			for (; first != last; ++first, ++inserted)
			{
				new(temp + index + inserted)T(*first);
				++constructed;
			}

			if (inserted != count)
				throw std::runtime_error("inserted != count for some reason");

			if (preferMove)
			{
				//move from [index, end)
				for (std::size_t oldIndex{ index }; oldIndex < m_CurrentSize; ++oldIndex)
				{
					new(temp + oldIndex + count)T(std::move(m_Arr[oldIndex]));
					++constructed;
				}
			}
			else
			{
				//copy from [index, end)
				for (std::size_t oldIndex{ index }; oldIndex < m_CurrentSize; ++oldIndex)
				{
					new(temp + oldIndex + count)T(m_Arr[oldIndex]);
					++constructed;
				}
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < constructed; ++i)
				temp[i].~T();
			::operator delete(temp);

			throw;
		}

		//free m_Arr
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();
		::operator delete(m_Arr);

		//install new
		m_Arr = temp;
		m_CurrentSize = newSize;
		m_Capacity = newCapacity;
		return m_Arr + index;
	}
//================================================================================================================================//
public:
	//Default constructor
	Vector()
		:m_Arr(nullptr), m_CurrentSize(0), m_Capacity(2)
	{
		m_Arr = reinterpret_cast<T*>(::operator new(m_Capacity * sizeof(T)));
	}

	//Parametrized constructor
	Vector(std::initializer_list<T> list)
		:m_Arr(nullptr), m_CurrentSize(0), m_Capacity(list.size())
	{
		T* temp = reinterpret_cast<T*>(::operator new(m_Capacity * sizeof(T)));
		std::size_t tempSize = 0;

		try
		{
			for (const T& e : list)
			{
				new(temp + tempSize)T(e);
				++tempSize;
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < tempSize; ++i)
				temp[i].~T();

			::operator delete(temp);
			throw;
		}

		m_Arr = temp;
		m_CurrentSize = tempSize;
	}

	//copy assignment operator
	Vector& operator= (const Vector& other)
	{
		if (this == &other)
			return *this;

		//Allocate storage in temp in case of throw
		T* temp = reinterpret_cast<T*>(::operator new(other.m_Capacity * sizeof(T)));
		std::size_t tempSize = 0;

		try 
		{
			//Copy construct into temp
			for (std::size_t i{ 0 }; i < other.m_CurrentSize; ++i)
			{
				new(temp + i) T(other.m_Arr[i]);
				++tempSize;
			}
		}

		catch (...)
		{
			//destruct temp elements
			for (std::size_t i{ 0 }; i < tempSize; ++i)
				temp[i].~T();

			//free raw memory
			::operator delete(temp);

			//rethrow
			throw;
		}

		//Destroy old, free old, install new
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		::operator delete(m_Arr);

		m_Arr = temp;
		m_CurrentSize = tempSize;
		m_Capacity = other.m_Capacity;

		return *this;
	}

	//Move assignment operator
	Vector& operator= (Vector&& other) noexcept
	{
		if (this == &other)
			return *this;

		if (m_Arr != nullptr)
		{
			for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
				m_Arr[i].~T();

			::operator delete(m_Arr);
		}

		m_Arr = other.m_Arr;
		m_Capacity = other.m_Capacity;
		m_CurrentSize = other.m_CurrentSize;

		other.m_Arr = nullptr;
		other.m_CurrentSize = 0;
		other.m_Capacity = 0;

		return *this;
	}


	//Copy constructor
	Vector(const Vector& other)
		:m_Arr(nullptr), m_CurrentSize(0), m_Capacity(other.m_Capacity)
	{
		std::size_t constructed = 0;

		m_Arr = reinterpret_cast<T*>(::operator new(other.m_Capacity * sizeof(T)));
		
		try 
		{
			for (; constructed < other.m_CurrentSize; ++constructed)
				new(m_Arr + constructed)T(other.m_Arr[constructed]);

			m_CurrentSize = other.m_CurrentSize;
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < constructed; ++i)
				m_Arr[i].~T();

			::operator delete(m_Arr);
			m_Arr = nullptr;
			throw;
		}
	}

	//Move constructor
	Vector(Vector&& other) noexcept
		:m_Arr(other.m_Arr), m_CurrentSize(other.m_CurrentSize), m_Capacity(other.m_Capacity)
	{
		other.m_Arr = nullptr;
		other.m_CurrentSize = 0;
		other.m_Capacity = 0;
	}

	//Size constructor
	Vector(std::size_t n)
		:m_Arr(nullptr), m_CurrentSize(0), m_Capacity(n)
	{
		m_Arr = reinterpret_cast<T*>(::operator new(m_Capacity * sizeof(T)));

		//default construct n T's in m_Arr
		try
		{
			for (std::size_t i{ 0 }; i < n; ++i)
			{
				new(m_Arr + i)T();
				++m_CurrentSize;
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
				m_Arr[i].~T();

			::operator delete(m_Arr);
			throw;
		}
	}

	//Fill Constructor
	Vector(std::size_t count, const T& value)
		:m_Arr(nullptr), m_CurrentSize(0), m_Capacity(count)
	{
		m_Arr = reinterpret_cast<T*>(::operator new(m_Capacity * sizeof(T)));

		try
		{
			for (std::size_t i{ 0 }; i < count; ++i)
			{
				new(m_Arr + i)T(value);
				++m_CurrentSize;
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
				m_Arr[i].~T();

			::operator delete(m_Arr);
			throw;
		}
	}

	//Destructor
	~Vector()
	{
		//call destructor of each constructed element
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		//free raw memory block ONCE
		::operator delete(m_Arr);
	}

//================================================================================================================================//
	T* begin() 
	{
		if (m_Arr == nullptr)
			return nullptr;
		else
			return m_Arr;
	}

	T* end() 
	{ 
		if (m_Arr == nullptr)
			return nullptr;
		else
			return (m_Arr + m_CurrentSize); 
	}

	//const overloads for const object calls
	const T* begin() const
	{
		if (m_Arr == nullptr)
			return nullptr;
		else
			return m_Arr;
	}

	const T* end() const
	{
		if (m_Arr == nullptr)
			return nullptr;
		else
			return (m_Arr + m_CurrentSize);
	}
													//Element access//
//================================================================================================================================//
	T& operator[](std::size_t index)
	{
		if (index >= m_CurrentSize)
			throw std::runtime_error("Out of bounds index");
		else
			return m_Arr[index];
	}

	const T& operator[](std::size_t index) const { return m_Arr[index]; }

	T& front() { return this->at(0); }

	T& back() { return this->at(m_CurrentSize - 1); }

	T* data() { return m_Arr; }
//================================================================================================================================//
// 
													//Capacity//
//================================================================================================================================//
	bool empty() const { return (m_CurrentSize == 0); }

	std::size_t size() const { return m_CurrentSize; }

	//theoretical limit to how many Ts a Vector can contain
	std::size_t max_size() const { return std::numeric_limits<std::size_t>::max() / sizeof(T); }

	bool reserve(std::size_t new_cap)
	{
		if (new_cap <= m_Capacity)
			return false;

		else
			Reallocate(new_cap);

		return true;
	}

	std::size_t capacity() const { return m_Capacity; }

	//reduce capacity to exactly how many elements are currently in the container. if size == capacity do nothing. if the size is 0 and shrink is called, free the memory and zero everything.
	void shrink_to_fit()
	{
		if (m_CurrentSize == m_Capacity)
			return;

		if (m_CurrentSize == 0)
		{
			::operator delete(m_Arr);

			m_Arr = nullptr;
			m_CurrentSize = 0;
			m_Capacity = 0;

			return;
		}

		T* temp = reinterpret_cast<T*>(::operator new(m_CurrentSize * sizeof(T)));
		std::size_t tempSize = 0;

		try
		{
			for (; tempSize < m_CurrentSize; ++tempSize)
			{
				new(temp + tempSize)T(m_Arr[tempSize]);
			}
		}

		catch (...)
		{
			for (std::size_t i{ 0 }; i < tempSize; ++i)
				temp[i].~T();
			::operator delete(temp);

			throw;
		}

		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();
		::operator delete(m_Arr);

		m_Arr = temp;
		m_Capacity = m_CurrentSize;
	}

//Zero everything, including capacity
	void reset()
	{
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		::operator delete(m_Arr);

		m_Arr = nullptr;
		m_Capacity = 0;
		m_CurrentSize = 0;

	}
//=====================================================================================================================================

													//Modifiers//
//=====================================================================================================================================
	//Keep capacity
	void clear()
	{
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		m_CurrentSize = 0;
	}

	//will always copy since non-universal reference (only l-values)
	T* insert(const T* pos, const T& value)
	{
		std::size_t index;

		if (m_Arr == nullptr)
			if (pos != nullptr)
				throw std::runtime_error("Empty array must be indexed at 0");
			else
				index = 0;
		else
		{
			//ensure pos ptr is in [begin, end]
			if (pos < m_Arr || pos > m_Arr + m_CurrentSize)
				throw std::runtime_error("pos not in Vector");
			else
				index = static_cast<std::size_t>(pos - m_Arr);
		}

		//requires reallocation branch
		if ((m_CurrentSize + 1) > m_Capacity)
		{			
			std::size_t newCapacity;

			//if old capacity is < 2, allocate + old cap + 1 capacity, otherwise grow by 50%
			if (m_Capacity < 2)
				newCapacity = m_Capacity + 1;
			else
				newCapacity = m_Capacity + (m_Capacity / 2);

			T* temp = reinterpret_cast<T*>(::operator new(newCapacity * sizeof(T)));
			std::size_t tempSize = 0;

			try
			{
				//construct upto insertion point from m_Arr
				for (; tempSize < index; ++tempSize)
					new(temp + tempSize)T(m_Arr[tempSize]);

				//construct value at index
				new(temp + index)T(value);
				++tempSize;

				//suffix is shifted right by 1, so source index is dest - 1
				for (; tempSize < m_CurrentSize + 1; ++tempSize)
					new(temp + tempSize)T(m_Arr[tempSize - 1]);
			}

			catch (...)
			{
				for (std::size_t i{ 0 }; i < tempSize; ++i)
					temp[i].~T();
				::operator delete(temp);

				throw;
			}

			//free up m_Arr
			for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
				m_Arr[i].~T();
			::operator delete(m_Arr);

			m_Arr = temp;
			++m_CurrentSize;
			m_Capacity = newCapacity;

			//return pointer to the inserted value (memory of index + offset of first element in the Vector)
			return m_Arr + index;
		}

		//no reallocation required branch
		else
		{
			//if we are constructing at the end of the Vector
			if (index == m_CurrentSize)
			{
				new(&m_Arr[m_CurrentSize])T(value);
				++m_CurrentSize;

				return m_Arr + index;
			}

			//not constructing at the end of the Vector
			else
			{
				std::size_t oldSize = m_CurrentSize;

				//construct previous last value at end of arr
				new (&m_Arr[oldSize])T(m_Arr[oldSize - 1]);

				//shift elements one step backward from the old size to one before index
				for (std::size_t i{oldSize - 1}; i > (index); --i)
					m_Arr[i] = m_Arr[i - 1];

				m_Arr[index] = value;
				++m_CurrentSize;
				
				return m_Arr + index;
			}
		}
	}

	//overload for move construction
	T* insert(const T* pos, T&& value)
	{
		std::size_t index;

		//if m_Arr is empty, must index at 0
		if (m_Arr == nullptr)
			if (pos != nullptr)
				throw std::runtime_error("Empty array must be indexed at 0");
			else
				index = 0;
		else
		{
			//ensure pos ptr is in [begin, end]
			if (pos < m_Arr || pos > m_Arr + m_CurrentSize)
				throw std::runtime_error("pos not in Vector");
			else
				index = static_cast<std::size_t>(pos - m_Arr);
		}

		//requires reallocation branch
		if ((m_CurrentSize + 1) > m_Capacity)
		{
			std::size_t newCapacity;

			//if old capacity is < 2, allocate + old cap + 1 capacity, otherwise grow by 50%
			if (m_Capacity < 2)
				newCapacity = m_Capacity + 1;
			else
				newCapacity = m_Capacity + (m_Capacity / 2);

			T* temp = reinterpret_cast<T*>(::operator new(newCapacity * sizeof(T)));
			std::size_t tempSize = 0;

			try
			{
				//construct upto insertion point from m_Arr
				for (; tempSize < index; ++tempSize)
					new(temp + tempSize)T(std::move_if_noexcept(m_Arr[tempSize]));

				//construct value at index, prefer move since user is explicity passing the rvalue even if it can throw
				new(temp + index)T(std::move(value));
				++tempSize;

				//suffix is shifted right by 1, so source index is dest - 1
				for (; tempSize < m_CurrentSize + 1; ++tempSize)
					new(temp + tempSize)T(std::move_if_noexcept(m_Arr[tempSize - 1]));
			}

			catch (...)
			{
				for (std::size_t i{ 0 }; i < tempSize; ++i)
					temp[i].~T();
				::operator delete(temp);

				throw;
			}

			//free up m_Arr
			for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
				m_Arr[i].~T();
			::operator delete(m_Arr);

			m_Arr = temp;
			++m_CurrentSize;
			m_Capacity = newCapacity;

			//return pointer to the inserted value (memory of index + offset of first element in the Vector)
			return m_Arr + index;
		}

		//no reallocation required branch
		else
		{
			//if we are constructing at the end of the Vector
			if (index == m_CurrentSize)
			{
				new(&m_Arr[m_CurrentSize])T(std::move(value));
				++m_CurrentSize;

				return m_Arr + index;
			}

			//not constructing at the end of the Vector
			else
			{
				std::size_t oldSize = m_CurrentSize;

				//construct previous last value at end of arr
				new (&m_Arr[oldSize])T(std::move_if_noexcept(m_Arr[oldSize - 1]));

				//shift elements one step backward from the old size to one before index
				for (std::size_t i{ oldSize - 1 }; i > (index); --i)
					m_Arr[i] = std::move_if_noexcept(m_Arr[i - 1]);

				m_Arr[index] = std::move(value);
				++m_CurrentSize;

				return m_Arr + index;
			}
		}
	}

	//overload to take a size_t pos for insert
	T* insert(std::size_t pos, const T& value)
	{
		if (pos > m_CurrentSize)
			throw std::runtime_error("insert pos out of range");

		//convert pos to ptr via m_Arr + pos so other overloads can run it
		return insert(m_Arr + pos, value);
	}

	template <typename Container>
	T* insert_range(const T* pos, Container&& container)
	{
		using C = std::remove_cvref_t<Container>;

		std::size_t index;
		std::size_t count = 0;

		//if m_Arr is empty, must index at 0
		if (m_Arr == nullptr)
			if (pos != nullptr)
				throw std::runtime_error("Empty array must be indexed at 0");
			else
				index = 0;
		else
		{
			//ensure pos ptr is in [begin, end]
			if (pos < m_Arr || pos > m_Arr + m_CurrentSize)
				throw std::runtime_error("pos not in Vector");
			else
				index = static_cast<std::size_t>(pos - m_Arr);
		}

		//for checking if the container is "sized"
		Vector<T> buffer;

		//container is sized
		if constexpr (std::ranges::sized_range<C>)
		{
			count = std::ranges::size(container);

			//empty input
			if (count == 0)
				return m_Arr ? (m_Arr + index) : nullptr;

			//self insert overlap check only when data() can be reliably fetched
			if (m_Arr && count > 0)
			{
				if constexpr (std::ranges::contiguous_range<C> && std::ranges::sized_range<C>)
				{
					//std::ranges::data(x) returns a T* pointing to first element
					const T* src = std::ranges::data(container);
					//std::ranges::size(x) returns number of elements, so first element + size = ptr to last element
					const T* src_end = src + std::ranges::size(container);

					const T* dst = m_Arr;
					const T* dst_end = m_Arr + m_CurrentSize;

					//overlap if ranges interset [src, src_end) vs [dst, dst_end)
					const bool overlaps = (src < dst_end) && (dst < src_end);

					if (overlaps)
					{
						buffer.reserve(count);

						for (auto it{ std::begin(container) }; it != std::end(container); ++it)
							buffer.emplace_back(*it);

						return insert_range_impl(index, buffer.size(), buffer.begin(), buffer.end());
					}
				}
			}

			auto first = std::begin(container);
			auto last = std::end(container);
			return insert_range_impl(index, count, first, last);
		}
		else
		{
			auto first = std::begin(container);
			auto last = std::end(container);

			//unsized case: fill buffer so we know count and have stable iterators
			for (auto it = first; it != last; ++it)
				buffer.emplace_back(*it);

			count = buffer.size();

			if(count == 0)
				return m_Arr ? (m_Arr + index) : nullptr;

			return insert_range_impl(index, count, buffer.begin(), buffer.end());
		}
	}

	//construct element in place
	template <typename... Args>
	T* emplace(std::size_t pos, Args&&... args)
	{
		const bool preferMove = (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>);
			
		//no reallocation required
		if (m_CurrentSize + 1 <= m_Capacity)
		{
			//constructing at end
			if (pos == m_CurrentSize)
			{
				new(&m_Arr[m_CurrentSize])T(std::forward<Args>(args)...);
				++m_CurrentSize;
				
				return m_Arr + pos;
			}
			//not constructing at end
			else
			{
				//basic guarantee, moves and noexcept, no need for try catch
				if (preferMove)
				{
					//move construct last element in empty tail slot, then destroy element in that mem slot
					new(&m_Arr[m_CurrentSize])T(std::move(m_Arr[m_CurrentSize - 1]));
					m_Arr[m_CurrentSize - 1].~T();

					//shift elements backward by one
					for (std::size_t i{ m_CurrentSize - 1 }; i > pos; --i)
					{
						//move construct to element i
						new(&m_Arr[i])T(std::move(m_Arr[i - 1]));
						//destroy old element
						m_Arr[i - 1].~T();
					}

					new(&m_Arr[pos])T(std::forward<Args>(args)...);

					++m_CurrentSize;
					return m_Arr + pos;
				}
				//no preferMove, so copy into temp buffer for exception safety
				else
				{
					std::size_t idk = 0;

					T* temp = reinterpret_cast<T*>(::operator new((m_CurrentSize + 1) * sizeof(T)));

					try
					{
						//construct [0,pos)
						for (; idk < pos; ++idk)
							new(&temp[idk])T(m_Arr[idk]);

						//construct new element at pos
						new(&temp[idk])T(std::forward<Args>(args)...);
						++idk;

						//construct remaining elements from m_Arr into temp
						for (std::size_t i{ pos }; i < m_CurrentSize; ++i, ++idk)
							new(&temp[idk])T(m_Arr[i]);
					}
					catch (...)
					{
						//free temp mem
						for (std::size_t i{ 0 }; i < idk; ++i)
							temp[i].~T();
						::operator delete(temp);
						
						//rethrow
						throw;
					}
					//free m_Arr mem
					for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
						m_Arr[i].~T();
					::operator delete(m_Arr);

					m_Arr = temp;
					++m_CurrentSize;

					return m_Arr + pos;
				}
			}
		}
		//reallocation required
		else
		{
			//constructing at end
			if (pos == m_CurrentSize)
			{
				T* temp = reinterpret_cast<T*>(::operator new((m_CurrentSize + 1) * sizeof(T)));
				std::size_t newSize = 0;

				try
				{
					if(preferMove)
					{
						//copy construct all elements from m_Arr into temp
						for (newSize; newSize < m_CurrentSize; ++newSize)
							new(temp + newSize)T(std::move(m_Arr[newSize]));
					}

					else
					{
						//copy construct all elements from m_Arr into temp
						for (newSize; newSize < m_CurrentSize; ++newSize)
							new(temp + newSize)T(m_Arr[newSize]);
					}

					//copy construct parameter passed at end of temp
					new(temp + newSize)T(std::forward<Args>(args)...);
					++newSize;
				}
				catch (...)
				{
					for (std::size_t i{ 0 }; i < newSize; ++i)
						temp[i].~T();
					::operator delete(temp);

					throw;
				}

					//free up m_Arr before commit
					for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
						m_Arr[i].~T();
					::operator delete(m_Arr);

					m_Arr = temp;
					m_CurrentSize = newSize;
					m_Capacity = newSize;

					return m_Arr + pos;
			}
			else
			{
				//NOT constructing at end and preferMove
				if (preferMove)
				{
					T* temp = reinterpret_cast<T*>(::operator new((m_CurrentSize + 1) * sizeof(T)));
					std::size_t newSize = 0;

					try
					{
						//move construct up to pos from m_Arr into temp
						for (; newSize < pos; ++newSize)
							new(temp + newSize)T(std::move(m_Arr[newSize]));

						//construct parameter into pos
						new(temp + newSize)T(std::forward<Args>(args)...);
						++newSize;

						//move construct remaining elements from m_Arr into temp
						for (; newSize < m_CurrentSize + 1; ++newSize)
							new(temp + newSize)T(std::move(m_Arr[newSize - 1]));
					}
					catch (...)
					{
						for (std::size_t i{ 0 }; i < newSize; ++i)
							temp[i].~T();
						::operator delete(temp);

						throw;
					}

					//free up m_Arr
					for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
						m_Arr[i].~T();
					::operator delete(m_Arr);

					//commit
					m_Arr = temp;
					m_CurrentSize = newSize;
					m_Capacity = newSize;

					return m_Arr + pos;
				}

				//NOT constructing at end and NOT preferMove
				else
				{
					T* temp = reinterpret_cast<T*>(::operator new((m_CurrentSize + 1) * sizeof(T)));
					std::size_t newSize = 0;

					try
					{
						//copy construct up to pos from m_Arr into temp
						for (; newSize < pos; ++newSize)
							new(temp + newSize)T(m_Arr[newSize]);

						//construct parameter into pos
						new(temp + newSize)T(std::forward<Args>(args)...);
						++newSize;

						//construct remaining elements from m_Arr into temp
						for (; newSize < m_CurrentSize + 1; ++newSize)
							new(temp + newSize)T(m_Arr[newSize - 1]);
					}
					catch (...)
					{
						for (std::size_t i{ 0 }; i < newSize; ++i)
							temp[i].~T();
						::operator delete(temp);

						throw;
					}

					for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
						m_Arr[i].~T();
					::operator delete(m_Arr);

					m_Arr = temp;
					m_CurrentSize = newSize;
					m_Capacity = newSize;

					return m_Arr + pos;
				}
			}
		}	
	}

	//overload to accept a pointer as pos
	template <typename... Args>
	T* emplace(const T* pos, Args&&... args)
	{
		if (pos < m_Arr || (pos > m_Arr + m_CurrentSize))
			throw std::runtime_error("pos not in Vector");

		std::size_t index = static_cast<std::size_t>(pos - m_Arr);
		return emplace(index, std::forward<Args>(args)...);
	}

	T* erase(const T* pos)
	{
		//convert pos to index to work with
		std::size_t index = static_cast<std::size_t>(pos - m_Arr);
		
		//bounds check
		if (index > m_CurrentSize)
			throw std::runtime_error("Index out of bounds");

		std::size_t oldSize = m_CurrentSize;
		
		//shift beyond index left one
		for (std::size_t i{ index }; i + 1 < oldSize; ++i)			//	//[1][2][3][4][5][6] -> //[1][2][3][5][6][6] -> [1][2][3][5][6]
			m_Arr[i] = std::move(m_Arr[i + 1]);

		//destroy old last element
		m_Arr[oldSize - 1].~T();
		--m_CurrentSize;

		return m_Arr + index;
	}

	//overload to erase iterator range of values
	T* erase(T* first, T* last)
	{
		//range is 0
		if (first == last)
			return first;

		std::size_t index_start = static_cast<std::size_t>(first - m_Arr);
		std::size_t index_end = static_cast<std::size_t>(last - m_Arr);

		if (index_start > m_CurrentSize || index_end > m_CurrentSize)
			throw std::runtime_error("Invalid erase range");

		std::size_t moves = index_end - index_start;
		std::size_t oldSize = m_CurrentSize;

		//shift tail left
		for (std::size_t i{index_end}; i < oldSize; ++i)				//[1][2][3][4][5][6] -> [1][4][5][6][5][6]
			m_Arr[i - moves] = std::move(m_Arr[i]);	

		for (std::size_t i{oldSize - moves}; i < oldSize; ++i)			//[1][4][5][6][x][x]
			m_Arr[i].~T();

		m_CurrentSize -= moves;

		return m_Arr + index_start;
	}

	//overload to erase at a specified index
	T* erase_index(std::size_t index)
	{
		T* pos = m_Arr + index;
		return erase(pos);
	}

	//add value at the end of the array
	void push_back(T val)
	{
		if (m_CurrentSize >= m_Capacity)
			Reallocate(m_Capacity + (m_Capacity / 2)); //Grow by 50%

		void* slot = &m_Arr[m_CurrentSize];

		new(slot)T(val);
		++m_CurrentSize;
	}

	template <typename... Args>
	T* emplace_back(Args&&... args)
	{
		if (m_Capacity == 0)
			Reallocate(2);

		if (m_CurrentSize >= m_Capacity)
			Reallocate(m_Capacity + (m_Capacity / 2)); //grow by 50%

		void* slot = &m_Arr[m_CurrentSize];

		new(slot) T(std::forward<Args>(args)...);
		++m_CurrentSize;

		return m_Arr + (m_CurrentSize - 1);
	}

	//appends a list of values to the tail of the Vector
	template <typename Container>
	T* append_range(Container&& container)
	{
		const T* pos;

		if (m_Arr == nullptr)
			pos = nullptr;
			
		else
			pos = m_Arr + m_CurrentSize;

		return insert_range(pos, std::forward<Container>(container));
	}

	void pop_back()
	{
		if (m_CurrentSize == 0)
			throw std::runtime_error("Cannot pop_back an empty Vector");

		m_Arr[m_CurrentSize - 1].~T();
		--m_CurrentSize;
	}

	void resize(std::size_t count)
	{

		if (m_CurrentSize > count)
		{
			while (m_CurrentSize > count)
			{
				--m_CurrentSize;
				m_Arr[m_CurrentSize].~T();
			}
		}

		else if (m_CurrentSize < count)
		{
			std::size_t oldSize = m_CurrentSize;

			if (m_Capacity < count)
				Reallocate(count);
			try
			{
				for (; m_CurrentSize < count; ++m_CurrentSize)
					new(m_Arr + m_CurrentSize)T();
			}
			catch (...)
			{
				for (std::size_t i{ oldSize }; i < m_CurrentSize; ++i)
					m_Arr[i].~T();

				m_CurrentSize = oldSize;
				throw;
			}
		}

		//count is equal to the current size
		else
			return;
	}

	void swap(Vector& other)
	{
		//early return
		if (this == &other)
			return;

		T* temp = m_Arr;
		std::size_t tempSize = m_CurrentSize;
		std::size_t tempCapacity = m_Capacity;

		m_Arr = other.m_Arr;
		m_CurrentSize = other.m_CurrentSize;
		m_Capacity = other.m_Capacity;

		other.m_Arr = temp;
		other.m_CurrentSize = tempSize;
		other.m_Capacity = tempCapacity;
	}

	T& at(std::size_t index)
	{
		if (index >= m_CurrentSize)
			throw std::runtime_error("Indexed out of bounds");

		return m_Arr[index];
	}
//================================================================================================================================//

														//Additional Functions//
//================================================================================================================================//
	void print() const
	{
		std::cout << '(';
		auto seperator = "";

		for (const auto& e : *this)
		{
			std::cout << seperator << e;
			seperator = ", ";
		}
		std::cout << ')' << "\n";
	}
};
//================================================================================================================================//
// 
//=====================
//CTAD deduction guide
//=====================

//initalizer list deduction
template <typename T>
Vector(std::initializer_list<T>) -> Vector<T>;

//iterator pair deduction
template <typename It>
Vector(It, It) -> Vector<typename std::iterator_traits<It>::value_type>;

//size + value deduction
template <typename T>
Vector(std::size_t, T) -> Vector<T>;

															//Non-member comparison functions//
//================================================================================================================================//
template <typename T>
bool operator==(const Vector<T>& lhs, const Vector<T>& rhs)
{
	//check current size
	if (lhs.size() != rhs.size())
		return false;

	//check each element
	for (std::size_t i{ 0 }; i < lhs.size(); ++i)
		if (!(lhs[i] == rhs[i]))
			return false;

	return true;
}

template <typename T>
bool operator !=(const Vector<T>& lhs, const Vector<T>& rhs) { return !(lhs == rhs); }

template <typename T>
bool operator <(const Vector<T>& lhs, const Vector<T>& rhs)
{
	for (std::size_t i{ 0 }; i < std::min(lhs.size(), rhs.size()); ++i)
	{
		if (lhs[i] < rhs[i])
			return true;
		if (rhs[i] < lhs[i])
			return false;
	}

	return (lhs.size() < rhs.size());
}

template <typename T>
bool operator >(const Vector<T>& lhs, const Vector<T>& rhs) { return (rhs < lhs); }

template <typename T>
bool operator <=(const Vector<T>& lhs, const Vector<T>& rhs) { return !(rhs < lhs); }

template <typename T>
bool operator >=(const Vector<T>& lhs, const Vector<T>& rhs) { return !(lhs < rhs); }
//================================================================================================================================//

#endif
