#ifndef __UTIL_ARRAY__
#define __UTIL_ARRAY__

#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <new>

template <typename T>
class Array
{
public:
	Array();
	explicit Array(Array<T> &o);
	explicit Array(uint32_t size);
	Array(uint32_t size, uint32_t capacity);
	~Array();
	
	uint32_t	size() const;
	uint32_t	capacity() const;
	void		add(T const &item);
	void		removeLast();
	void		clear();
	T			&operator[](uint32_t index);
	const T		&operator[](uint32_t index) const;
	void		copyTo(T* p);
	
private:
	T*			_data;
	uint32_t	_size;
	uint32_t	_capacity;
};

template <typename T>
Array<T>::Array()
	: _data(0)
	, _size(0)
	, _capacity(0)
{
}

template <typename T>
Array<T>::Array(Array<T> &o)
	: _size(o._size)
	, _capacity(o._size)
{
	_data = (T*) malloc(sizeof (T) * _capacity);
	for (uint32_t i = 0; i < _size; i ++)
	{
		new (_data + i) T(o._data[i]);
	}
}

template <typename T>
Array<T>::Array(uint32_t size)
	: _size(size)
	, _capacity(size)
{
	_data = (T*) malloc(sizeof (T) * _capacity);
	for (uint32_t i = 0; i < _size; i ++)
	{
		new (_data + i) T();
	}
}

template <typename T>
Array<T>::Array(uint32_t size, uint32_t capacity)
	: _size(size)
	, _capacity(capacity)
{
	_data = (T*) malloc(sizeof (T) * _capacity);
	for (uint32_t i = 0; i < _size; i ++)
	{
		new (_data + i) T();
	}
}

template <typename T>
Array<T>::~Array()
{
	clear();
}

template <typename T>
uint32_t Array<T>::size() const
{
	return _size;
}

template <typename T>
uint32_t Array<T>::capacity() const
{
	return _capacity;
}

template <typename T>
void Array<T>::add(T const &item)
{
	if (_size >= _capacity)
	{
		if (_capacity == 0)
		{
			_capacity = 2;
			_data = (T*) malloc(sizeof (T) * _capacity);
		}
		else
		{
			uint32_t uml = _capacity;
			uml --;
			uml |= uml >> 1;
			uml |= uml >> 2;
			uml |= uml >> 4;
			uml |= uml >> 8;
			uml |= uml >> 16;
			uml ++;

			_capacity = (uml > _capacity) ? uml : _capacity * 2;
			_data = (T*) realloc(_data, sizeof (T) * _capacity);
		}
	}
	
	new (_data + _size) T(item);
	_size ++;
}

template <typename T>
void Array<T>::removeLast()
{
	if (_size <= 0)
		return;
	
	(_data + _size - 1)->~T();
	_size --;
	
	if (_size == 0)
	{
		clear();
	}
	else if (_size * 3 < _capacity)
	{
		uint32_t uml = _capacity / 2;
		uml --;
		uml |= uml >> 1;
		uml |= uml >> 2;
		uml |= uml >> 4;
		uml |= uml >> 8;
		uml |= uml >> 16;
		uml ++;
		
		_capacity = (uml < (_capacity / 2)) ? uml : (_capacity / 2);
		if (_capacity < _size)
			_capacity = _size;
			
		_data = (T*) realloc(_data, sizeof (T) * _capacity);
	}
}

template <typename T>
void Array<T>::clear()
{
	for (uint32_t i = 0; i < _size; i ++)
	{
		(_data + i)->~T();
	}
	free(_data);
	_data = 0;
	_size = 0;
	_capacity = 0;
}

template <typename T>
T &Array<T>::operator[](uint32_t index)
{
	assert(index < _size);
	return _data[index];
}

template <typename T>
const T &Array<T>::operator[](uint32_t index) const
{
	assert(index < _size);
	return _data[index];
}

template <typename T>
void Array<T>::copyTo(T* p)
{
	memcpy(p, _data, sizeof (T) * _size);
}

#endif	// __UTIL_ARRAY__
