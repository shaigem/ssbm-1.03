#include "hsd/heap.h"
#include "hsd/memory.h"
#include "os/heap.h"
#include "os/os.h"
#include "util/mempool.h"
#include <cstddef>
#include <new>

extern "C" char __LOAD_BASE__;
constexpr void *load_base = &__LOAD_BASE__;

static char heap_data[0x8000] [[gnu::aligned(32)]];
static auto heap = -1;

static s32 get_heap()
{
	// Make our own heap with blackjack and hookers so any additional mods don't have less free
	// heap space than expected
	if (heap == -1)
		heap = OSCreateHeap(heap_data, heap_data + sizeof(heap_data));

	return heap;
}

void *malloc(size_t count)
{
	auto *ptr = OSAllocFromHeap(get_heap(), count);

	// Fall back to main heap
	if (ptr == nullptr)
		ptr = HSD_MemAlloc(count);

	return ptr;
}

void free(void *ptr)
{
	// Check which heap to free to
	if ((uintptr_t)ptr - (uintptr_t)heap_data < sizeof(heap_data))
		OSFreeToHeap(get_heap(), ptr);
	else
		HSD_Free(ptr);
}

void *operator new(size_t count)
{
	return malloc(count);
}

void *operator new[](size_t count)
{
	return malloc(count);
}

void operator delete(void *ptr)
{
	free(ptr);
}

void operator delete(void *ptr, size_t sz)
{
	free(ptr);
}

void operator delete[](void *ptr)
{
	free(ptr);
}

void operator delete[](void *ptr, size_t sz)
{
	free(ptr);
}

void fix_heap(OSHeap *heap)
{
	// Resize the heap to avoid overwriting mod code
	auto *chunk = heap->free_list;

	while (chunk != nullptr) {
		const auto *chunk_end = (char*)chunk + chunk->size;

		if (chunk >= load_base) {
			// Chunk is entirely within code space, unlink
			if (chunk->prev != nullptr)
				chunk->prev->next = chunk->next;
			else
				heap->free_list = chunk->next;

			if (chunk->next != nullptr)
				chunk->next->prev = chunk->prev;

			break;
		} else if (chunk_end > load_base) {
			// Chunk is partially within code space, reduce size
			chunk->size = (s32)load_base - (s32)chunk;
		}

		chunk = chunk->next;
	}
}

[[gnu::constructor]] static void manage_heaps()
{
	for (auto i = 0; i < MaxHeapCount; i++) {
		if (i == get_heap())
			continue;

		auto *heap = &HeapList[i];
		if (heap->size >= 0)
			fix_heap(heap);
	}

	if (hsd_heap_arena_hi > load_base)
		hsd_heap_arena_hi = load_base;

	if (default_hsd_heap_arena_hi > load_base)
		default_hsd_heap_arena_hi = load_base;

	for (auto i = 0; i < 6; i++) {
		auto *heap = &hsd_heap_list[i];
		auto *heap_end = (char*)heap->start + heap->size;
		if (heap_end > load_base)
			heap->size = (u32)load_base - (u32)heap->start;
	}
}

extern "C" void orig_HSD_ResetScene();
extern "C" void hook_HSD_ResetScene()
{
	// Reset 1.03 heap
	if (heap != -1) {
		OSDestroyHeap(heap);
		heap = -1;
	}

	// Run destructors and clear pool refcounts on heap destruction
	mempool::free_all();

	orig_HSD_ResetScene();
}