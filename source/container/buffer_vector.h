#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>


template<typename ValueType>
class pointer_forward_iterator : public std::iterator<std::input_iterator_tag, ValueType>
{
public:
	pointer_forward_iterator(const pointer_forward_iterator& it) : data_(it.data_) {}
	pointer_forward_iterator(ValueType* data) : data_(data) {}

	bool operator != (pointer_forward_iterator const& other) const
	{
		return this->data_ != other.data_;
	}
	bool operator == (pointer_forward_iterator const& other) const
	{
		return this->data_ == other.data_;
	}
	typename pointer_forward_iterator::reference operator*() const
	{
		return *this->data_;
	}
	pointer_forward_iterator operator + (size_t offset)
	{
		return pointer_forward_iterator(this->data_ + offset);
	}

	pointer_forward_iterator& operator++()
	{
		++this->data_;
		return *this;
	}
private:

	ValueType* data_;
};

template<typename T>
class buffer_vector
{
public:
	buffer_vector(T* buffer, size_t buffer_size)
	{
		this->capacity_ = buffer_size;
		this->data_ = buffer;
	}

	T& operator[](size_t index)
	{
		return this->data_[index];
	}
	T& at(size_t index)
	{
		if (index >= this->size())
			throw std::out_of_range("index");

		return this->data_[index];
	}

	[[nodiscard]] size_t size() const noexcept
	{
		return this->count_;
	}

	[[nodiscard]] size_t capacity() const noexcept
	{
		return this->capacity_;
	}

	[[nodiscard]] bool is_full() const
	{
		return this->count_ == this->capacity_;
	}
	[[nodiscard]] bool is_empty() const
	{
		return this->count_ == 0;
	}

	[[nodiscard]] T& front() const
	{
		return this->data_[0];
	}
	[[nodiscard]] T& back() const
	{
		return this->data_[this->count_ - 1];
	}

	bool push_back(T item) noexcept
	{
		if (this->is_full())
			return false;

		this->data_[this->count_++] = item;
		return true;
	}
	void pop_back()
	{
		if (this->count_ != 0)
			--this->count_;
	}

	bool insert(T item, size_t index)
	{
		if (this->is_full())
			return false;

		if (index == this->count_)
			return this->push_back(item);

		for (size_t i = this->count_; i > index; --i)
			this->data_[i] = this->data_[i - 1];

		this->data_[index] = item;

		++this->count_;
		return true;
	}
	bool remove(size_t index)
	{
		if (index >= this->count_)
			return false;

		for (size_t i = index; i < this->count_ - 1; i++)
			this->data_[i] = this->data_[i + 1];

		--this->count_;

		return true;
	}

	buffer_vector& operator = (buffer_vector&& arr) noexcept
	{
		this->data_ = std::move(arr.data_);
		this->capacity_ = arr.capacity_;
		this->count_ = arr.count_;

		return *this;
	}

	pointer_forward_iterator<T> begin()
	{
		return pointer_forward_iterator<T>(this->data_);
	}
	pointer_forward_iterator<T> end()
	{
		return pointer_forward_iterator<T>(this->data_ + this->count_);
	}

	~buffer_vector() = default;
protected:
	T* data_;
	size_t count_ = 0;
	size_t capacity_ = 0;
};
