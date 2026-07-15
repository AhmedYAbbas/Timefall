#include "tfpch.h"

// Reports every heap allocation to Tracy. Compiled into EACH module (engine DLL, editor,
// sandbox): operator new/delete bind per-image on Windows, so one copy per image is required.
#ifdef TRACY_ENABLE

#include <new>

namespace
{
	void* AllocReport(std::size_t size)
	{
		void* ptr = std::malloc(size);
		if (!ptr)
			throw std::bad_alloc{};
		TF_PROFILE_ALLOC(ptr, size);
		return ptr;
	}

	void* AlignedAllocReport(std::size_t size, std::size_t alignment)
	{
		void* ptr = _aligned_malloc(size, alignment);
		if (!ptr)
			throw std::bad_alloc{};
		TF_PROFILE_ALLOC(ptr, size);
		return ptr;
	}

	void FreeReport(void* ptr)
	{
		if (!ptr)
			return;
		TF_PROFILE_FREE(ptr);
		std::free(ptr);
	}

	void AlignedFreeReport(void* ptr)
	{
		if (!ptr)
			return;
		TF_PROFILE_FREE(ptr);
		_aligned_free(ptr);
	}
}

void* operator new(std::size_t size)
{
	return AllocReport(size);
}

void* operator new[](std::size_t size)
{
	return AllocReport(size);
}

void* operator new(std::size_t size, std::align_val_t align)
{
	return AlignedAllocReport(size, (std::size_t)align);
}

void* operator new[](std::size_t size, std::align_val_t align)
{
	return AlignedAllocReport(size, (std::size_t)align);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	void* ptr = std::malloc(size);
	if (ptr)
		TF_PROFILE_ALLOC(ptr, size);
	return ptr;
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
	return operator new(size, std::nothrow);
}

void operator delete(void* ptr) noexcept
{
	FreeReport(ptr);
}

void operator delete[](void* ptr) noexcept
{
	FreeReport(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
	FreeReport(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
	FreeReport(ptr);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
	AlignedFreeReport(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept
{
	AlignedFreeReport(ptr);
}

void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept
{
	AlignedFreeReport(ptr);
}

void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept
{
	AlignedFreeReport(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
	FreeReport(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
	FreeReport(ptr);
}

#endif
