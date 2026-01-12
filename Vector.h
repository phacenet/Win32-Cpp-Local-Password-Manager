#ifndef VECTOR_H
#define VECTOR_H

#include <cstddef>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

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

public:
//================================================================================================================================//
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
//================================================================================================================================//
	T& operator[](T index)
	{
		if (index >= m_CurrentSize)
			throw std::runtime_error("Out of bounds index");
		else
			return m_Arr[index];
	}

	//add value at the end of the array
	void push_back(T val)
	{
		if (m_CurrentSize >= m_Capacity)
			Reallocate(m_Capacity + (m_Capacity / 2)); //Grow by 50%

		m_Arr[m_CurrentSize] = val;
		++m_CurrentSize;
	}

	template <typename... Args>
	void emplace_back(Args&&... args)
	{
		if (m_CurrentSize >= m_Capacity)
			Reallocate(m_Capacity + (m_Capacity / 2)); //grow by 50%

		void* slot = &m_Arr[m_CurrentSize];

		new(slot) T(std::forward<Args>(args)...);
		++m_CurrentSize;
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

	//Keep capacity
	void clear()
	{
		for (std::size_t i{ 0 }; i < m_CurrentSize; ++i)
			m_Arr[i].~T();

		::operator delete(m_Arr);

		m_Arr = nullptr;
		m_CurrentSize = 0;
	}

	bool reserve(std::size_t new_cap)
	{
		if (new_cap <= m_Capacity)
			return false;

		else
			Reallocate(new_cap);

		return true;
	}

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

	bool empty() const {return (m_Arr == nullptr);}

	std::size_t size() const {return m_Capacity;}

	std::size_t capacity() const {return m_Capacity;}

	//theoretical limit to how many Ts a Vector can contain
	std::size_t max_size() const {return std::numeric_limits<std::size_t>::max() / sizeof(T);}




};


#endif
