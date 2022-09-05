#pragma once

#include <stdlib.h>
#include "array.h"

class String
{
public:
	String();
	String(const char*);
	String(const String &);
	~String();
	
	const char*	c_str() const;
	size_t		length() const;
	
	String		substr(size_t, size_t = 0) const;
	String		trim() const;
	String		term(size_t &) const;
	String		term(size_t &, const char) const;
	String		line(size_t &) const;

	String&		operator=(const String &);
	String&		operator=(const char*);
	String&		operator+=(const String &);
	String&		operator+=(const char*);
	String		operator+(const String &) const;
	String		operator+(const char*) const;
	bool		operator==(const String &) const;
	bool		operator==(const char*) const;
	bool		operator!=(const String &) const;
	bool		operator!=(const char*) const;
	char&		operator[](size_t);
	
	int			toInt() const;
	float		toFloat() const;

	size_t		indexOf(const char, size_t = 0) const;	
	size_t		lastIndexOf(const char, size_t = -1) const;	
	size_t		indexOf(const String &, size_t = 0) const;
	String		replace(const String &, const String &, size_t = 0) const;
	String		replaceAll(const String &, const String &) const;
    String      toLowerCase() const;
    String      toUpperCase() const;
	
	static Array<String>		split(const String &, const char);
	static String				join(Array<String> &, const String &);
	
private:
	String(char*, size_t len, size_t capacity);
	size_t		_length, _capacity;
	char*		_buf;
};

