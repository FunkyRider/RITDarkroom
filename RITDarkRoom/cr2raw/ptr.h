#ifndef __UTIL_PTR__
#define __UTIL_PTR__

#include <memory.h>
#include <assert.h>

template <typename T>

class Ptr
{
public:
	Ptr();
	explicit Ptr(T* o);
	Ptr(const Ptr<T> &p);
	~Ptr();
	T &operator* ();
	T* operator-> ();
	const T* operator-> () const;
	Ptr<T> &operator= (const Ptr<T> &p);
	operator bool() const;
	bool operator== (const Ptr<T>& p);

private:
	T*			_o;
	uint32_t*	_c;
};

template <typename T>
Ptr<T>::Ptr()
	: _o(0)
	, _c(0)
{
}

template <typename T>
Ptr<T>::Ptr(T* o)
	: _o(o)
	, _c(new uint32_t(1))
{
}

template <typename T>
Ptr<T>::Ptr(const Ptr<T> &p)
	: _o(p._o)
	, _c(p._c)
{
	(_c && ++ (*_c));
}

template <typename T>
Ptr<T>::~Ptr()
{
	if (_c && -- (*_c) == 0)
	{
		delete _o;
		delete _c;
	}
}

template <typename T>
T &Ptr<T>::operator* ()
{
	assert(_o != 0);
	return *_o;
}

template <typename T>
T* Ptr<T>::operator-> ()
{
	return _o;
}

template <typename T>
const T* Ptr<T>::operator-> () const
{
	return _o;
}


template <typename T>
Ptr<T> &Ptr<T>::operator= (const Ptr<T> &p)
{
	if (this != &p && _o != p._o)
	{
		if (_c && -- (*_c) == 0)
		{
			delete _o;
			delete _c;
		}
		
		_o = p._o;
		_c = p._c;
		
		(_c && ++ (*_c));
	}
	
	return *this;
}

template <typename T>
Ptr<T>::operator bool() const
{
	return (_o != 0);
}

template <typename T>
bool Ptr<T>::operator== (const Ptr<T>& p)
{
	return _o == p._o; 
}

#endif  //__UTIL_PTR__
