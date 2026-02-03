#ifndef UNIQUEPOINTER_H
#define UNIQUEPOINTER_H

#include <cstddef>
#include <utility>
#include <type_traits>

template <typename T>
class UniquePtr
{
private:
	T* m_ptr = nullptr;

public:
	//no copying since it is a unique ptr
	UniquePtr(const UniquePtr&) = delete;
	UniquePtr& operator= (const UniquePtr&) = delete;

	//Default constructor
	UniquePtr() = default;

	//T* constructor. prevent T* m_ptr = raw mem with explicit
	explicit UniquePtr(T* object)
		:m_ptr(object)
	{
	}

	//move constructor
	UniquePtr(UniquePtr&& other) noexcept
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	//deep move operator
	UniquePtr& operator= (UniquePtr&& other) noexcept
	{
		if (&other == this)
			return *this;

		delete m_ptr;

		m_ptr = other.m_ptr;
		other.m_ptr = nullptr;

		return *this;
	}

	//destructor
	~UniquePtr() noexcept
	{
		delete m_ptr;
	}

	//give up ownnership of the ptr and return the raw ptr
	T* release() noexcept
	{
		T* temp = m_ptr;
		m_ptr = nullptr;
		return temp;
	}

	//delete currently owned object (if any) and begin owning p. reset() just resets the underlying T* m_ptr, otherwise does prior
	void reset(T* p = nullptr) noexcept
	{
		delete m_ptr;
		m_ptr = p;
	}

	//swap ownership
	void swap(UniquePtr& other) noexcept
	{
		T* temp = other.m_ptr;

		other.m_ptr = m_ptr;
		m_ptr = temp;
	}

	//returns raw ptr, no transfer of ownership
	T* get() const noexcept { return m_ptr; }

	//dereference, assumes non null
	T& operator*() { return *m_ptr; }

	//overload for constness
	const T& operator*() const { return *m_ptr; }

	//member access. works like: UniquePtr<Foo> p; p->bar(); (m_ptr.operator->())->bar(); So simply supplying m_ptr to the compiler when -> is used is enough
	T* operator-> () { return m_ptr; }

	//overload for constness
	const T* operator-> () const { return m_ptr; }

	//return true if actively owning a ptr, false otherwise
	explicit operator bool() const noexcept { return (m_ptr != nullptr); }

	//allow for UniquePtr = nullptr. 
	UniquePtr& operator= (std::nullptr_t)
	{
		this->reset();
		return *this;
	}

	//comparisons for two UniquePtrs. True if they both point to same address
	bool operator== (const UniquePtr& other) const { return (m_ptr == other.m_ptr); }
	bool operator!= (const UniquePtr& other) const { return !(*this == other); }

	//comparisons for UniquePtr and nullptr
	bool operator== (std::nullptr_t) const { return m_ptr == nullptr; }
	bool operator!= (std::nullptr_t) const { return !(*this == nullptr); }
};

//for T[] to properly delete[]
template<typename T>
class UniquePtr<T[]>
{
private:
	T* m_ptr = nullptr;

public:
	//no copying since it is a unique ptr
	UniquePtr(const UniquePtr&) = delete;
	UniquePtr& operator= (const UniquePtr&) = delete;

	//Default constructor
	UniquePtr() = default;

	//T* constructor. prevent T* m_ptr = raw mem with explicit
	explicit UniquePtr(T* object)
		:m_ptr(object)
	{
	}

	//move constructor
	UniquePtr(UniquePtr&& other) noexcept
		: m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	//deep move operator
	UniquePtr& operator= (UniquePtr&& other) noexcept
	{
		if (&other == this)
			return *this;

		delete[] m_ptr;

		m_ptr = other.m_ptr;
		other.m_ptr = nullptr;

		return *this;
	}

	//destructor
	~UniquePtr() noexcept
	{
		delete[] m_ptr;
	}

	//give up ownnership of the ptr and return the raw ptr
	T* release() noexcept
	{
		T* temp = m_ptr;
		m_ptr = nullptr;
		return temp;
	}

	//delete currently owned object (if any) and begin owning p. reset() just resets the underlying T* m_ptr, otherwise does prior
	void reset(T* p = nullptr) noexcept
	{
		delete[] m_ptr;
		m_ptr = p;
	}

	//swap ownership
	void swap(UniquePtr& other) noexcept
	{
		T* temp = other.m_ptr;

		other.m_ptr = m_ptr;
		m_ptr = temp;
	}

	//returns raw ptr, no transfer of ownership
	T* get() const noexcept { return m_ptr; }

	//access operator
	T& operator[] (std::size_t index) noexcept { return m_ptr[index]; }

	//overload for const. non modifyable
	const T& operator[] (std::size_t index) const noexcept{ return m_ptr[index]; }

	//return true if actively owning a ptr, false otherwise
	explicit operator bool() const noexcept { return (m_ptr != nullptr); }

	//allow for UniquePtr = nullptr. 
	UniquePtr& operator= (std::nullptr_t)
	{
		this->reset();
		return *this;
	}

	//comparisons for two UniquePtrs. True if they both point to same address
	bool operator== (const UniquePtr& other) const { return (m_ptr == other.m_ptr); }
	bool operator!= (const UniquePtr& other) const { return !(*this == other); }

	//comparisons for UniquePtr and nullptr
	bool operator== (std::nullptr_t) const { return m_ptr == nullptr; }
	bool operator!= (std::nullptr_t) const { return !(*this == nullptr); }

};

//no need to mark free template functions as inline. use inline when:
/*
It’s a non-template function
It’s defined in a header
That header is included in multiple .cpp files
*/

//enable_if<condition, returnType>

//for single item. use if: NOT an array.
template <typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, UniquePtr<T>>
makeUnique(Args&&... args)
{
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

//for array version. remove_extent extracts T from T[]. Then make sure the array is unbounded, T[] NOT T[x]
//Without remove_extent, makeUnique<char[]>(len + 1) -> write new char[n] -> substitutes new char[][n]
template <typename T>
std::enable_if_t<std::is_array_v<T> && (std::extent_v<T> == 0), UniquePtr<T>>
makeUnique(std::size_t n)
{
	using Elem = std::remove_extent_t<T>;
	return UniquePtr<T>(new Elem[n]());
}




#endif