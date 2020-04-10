#pragma once

#include "container/buffer_vector.h"


template<typename T>
size_t difference(T* ptr1, T* ptr2)
{
	if (ptr1 < ptr2)
		return static_cast<size_t>(ptr2 - ptr1);

	return static_cast<size_t>(ptr1 - ptr2);
}

struct entity
{
	std::byte* ptr;
	size_t count;

	entity() : ptr(nullptr), count(0) {}
	entity(std::byte* ptr, size_t count) : ptr(ptr), count(count) {}

	[[nodiscard]] std::byte* end_ptr() const
	{
		return this->ptr + this->count;
	}
};

class entity_buffer : public buffer_vector<entity>
{
public:
	entity_buffer(entity* buffer, size_t buffer_size) : buffer_vector<entity>(buffer, buffer_size) {}

	bool insert_ordering(entity item)
	{
		size_t i = 0;

		for (; i < this->size(); i++)
			if (this->data_[i].ptr < item.ptr)
				break;

		return this->insert(item, i);
	}
	bool remove_item(entity item)
	{
		size_t i = 0;

		for (; i < this->size(); i++)
			if (this->data_[i].ptr == item.ptr)
				break;

		return this->remove(i);
	}
};

template<typename T>
class nfb_allocator // no fragmentation buffered allocator
{
public:
	nfb_allocator(std::byte* allocation_buffer, entity* entities_buffer, size_t allocation_buffer_size, size_t entity_buffer_size)
		: memory_buffer_(allocation_buffer), memory_buffer_size_(allocation_buffer_size), entities_(entities_buffer, entity_buffer_size)
	{}

	[[nodiscard]] T* allocate_c(size_t count = 1)
	{
		if (count == 0)
		{
			return nullptr;
		}

		const size_t requested_byte_count = count * sizeof(T);

		entity best_available_entity;
		entity only_one_entity; // use only in case 1

		switch (this->entities_.size())
		{
		case 0: // if no entities exist
			best_available_entity = { this->memory_buffer_, this->memory_buffer_size_ };
			break;
		case 1: // if a one entity exist
			only_one_entity = this->entities_.front();
			if (only_one_entity.ptr == this->memory_buffer_) // if entity placed front
			{
				// available place starts from end_ptr of entity, count is difference between memory buffer size and entity byte count
				best_available_entity = { only_one_entity.end_ptr(), this->memory_buffer_size_ - only_one_entity.count };
			}
			else if (only_one_entity.end_ptr() == this->end_buffer()) // if entity placed back
			{
				// available place starts from start of memory buffer, count is difference between memory buffer size and entity byte count
				best_available_entity = { this->memory_buffer_, this->memory_buffer_size_ - only_one_entity.count };
			}
			else // if entity placed in into memory
			{
				// first available entity placed left from existing entity
				const entity first_available_entity = { this->memory_buffer_, difference(this->memory_buffer_, only_one_entity.ptr) };

				// second available entity placed right from existing entity
				const entity last_available_entity = { only_one_entity.end_ptr(), difference(only_one_entity.end_ptr(), this->end_buffer()) };

				// write best entity
				best_available_entity = first_available_entity;
				if (last_available_entity.count >= requested_byte_count && last_available_entity.count < best_available_entity.count)
				{
					best_available_entity = last_available_entity;
				}
			}
			break;
		default: // if 2 or more entities existing
			entity first_available_entity;
			entity last_available_entity;

			const entity first = this->entities_.front(); // create an entity which points to the first entity
			const entity second = this->entities_[1]; // create an entity which points to the second entity
			const entity penultimate = this->entities_[this->entities_.size() - 2]; // create an entity which points to the penultimate entity
			const entity last = this->entities_.back(); // create an entity which points to the last entity

			// if the first entity is equal to the memory buffer
			if (first.ptr == this->memory_buffer_)
			{
				//first available place starts from end, count is difference between end and the second entity
				first_available_entity = { first.end_ptr(), difference(first.end_ptr(), second.ptr) };
			}
			else
			{
				//first available place starts from memory buffer, count is difference between memory buffer and the first entity
				first_available_entity = { this->memory_buffer_, difference(this->memory_buffer_, first.ptr) };
			}

			// if the last entity is equal to the end of buffer
			if (last.end_ptr() == this->end_buffer())
			{
				//last available place starts from penultimate entity, count is difference between penultimate end_ptr entity and the last entity
				last_available_entity = { penultimate.end_ptr(), difference(penultimate.end_ptr(), last.ptr) };
			}
			else
			{
				//last available place starts from last end entity, count is difference between last end entity and the end buffer
				last_available_entity = { last.end_ptr(), difference(last.end_ptr(), this->end_buffer()) };
			}

			best_available_entity = first_available_entity;

			// if the amount of last available entity is more than requested and less than best available entity
			if (last_available_entity.count >= requested_byte_count && last_available_entity.count < best_available_entity.count)
			{
				best_available_entity = last_available_entity;
			}

			break;
		}

		// if we have we more than two entities we have to search
		const bool need_to_foreach = this->entities_.size() >= 2;

		// we are looking for an available entity
		for (size_t i = 0; i < this->entities_.size() - 1 && need_to_foreach; i++)
		{
			const entity ent1 = this->entities_[i];
			const entity ent2 = this->entities_[i + 1];

			// create a current available entity which starts from ent1 end to difference between ent1 end and ent2 ptr
			const entity current_available_ent = { ent1.end_ptr(), difference(ent1.end_ptr(), ent2.ptr) };

			// if we have more than requested byte count
			if (current_available_ent.count < requested_byte_count)
			{
				continue;
			}

			// if best available entity is more than current available entity 
			if (best_available_entity.count > current_available_ent.count)
			{
				best_available_entity = current_available_ent;
			}
		}

		// if best available entity is less than requested 
		if (best_available_entity.count < requested_byte_count)
		{
			return nullptr;
		}

		// create filled entity of best available entity and requested byte count
		const entity filled_entity = { best_available_entity.ptr, requested_byte_count };
		this->entities_.insert_ordering(filled_entity); // inserting a filled entity

		return reinterpret_cast<T*>(filled_entity.ptr);
	}

	/**
	 * \brief creating an allocation
	 * \param count which gives us information about sent bytes of the memory
	 * \return ptr
	 */
	template<typename = std::is_constructible<T>>
	[[nodiscard]] T* allocate(size_t count = 1)
	{
		T* ptr = this->allocate_c(count);

		for (T* i = ptr; i < ptr + count && ptr != nullptr; ++i)
		{
			*i = T{};
		}

		return ptr;
	}

	/**
	 * \brief creating a deallocation
	 * \param ptr which gives information about an object
	 * \return
	 */
	bool deallocate(T* ptr)
	{
		auto entity_for_remove = std::find_if(entities_.begin(), entities_.end(), // creating an entity for remove from the begging to the end
			[ptr](const entity& ent)
			{
				return ent.ptr == reinterpret_cast<std::byte*>(ptr);
			});

		if (entity_for_remove == nullptr) // if entity is already removed
		{
			return false;
		}

		this->entities_.remove_item(*entity_for_remove);
		return true;
	}

private:
	std::byte* memory_buffer_;
	size_t memory_buffer_size_;

	entity_buffer entities_;

	[[nodiscard]] std::byte* end_buffer() const
	{
		return this->memory_buffer_ + this->memory_buffer_size_;
	}
};