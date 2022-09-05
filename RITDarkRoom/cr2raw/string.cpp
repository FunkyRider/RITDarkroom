#include <string.h>
#include "ustring.h"
#include <stdio.h>

String::String()
	: _length(0)
	, _capacity(0)
	, _buf(NULL)
{
}

String::String(const char* s)
{
	_length = s ? strlen(s) : 0;
	_capacity = _length + 1;
	
	if (_capacity > 1)
	{
		_buf = (char*) malloc(_capacity);
		strncpy(_buf, s, _capacity);
	}
	else
	{
		_capacity = 0;
		_length = 0;
		_buf = NULL;
	}
}

String::String(const String &m)
{
	_length = m._length;
	_capacity = _length + 1;
	
	if (_capacity > 1)
	{
		_buf = (char*) malloc(_capacity);
		strncpy(_buf, m._buf, _capacity);
	}
	else
	{
		_capacity = 0;
		_length = 0;
		_buf = NULL;
	}
}

String::~String()
{
	if (_capacity > 0)
	{
		free(_buf);
	}
}

const char* String::c_str() const
{
	return _buf ? _buf : "";
}

size_t String::length() const
{
	return _length;
}

String String::substr(size_t s, size_t l) const
{
	if (s >= _length)
	{
		return String();
	}
	
	if (l == 0 || l > _length - s)
	{
		l = _length - s;
	}
	
	char* buf = (char* ) malloc(l + 1);
	strncpy(buf, _buf + s, l);
	buf[l] = 0;
	
	return String(buf, l, l + 1);
}
   
String String::trim() const
{
	size_t s = 0, p = 0;
	
	while (p < _length)
	{
		const char ch = _buf[p];
		
		if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
		{
			break;
		}
		
		p ++;
	}
	
	s = p;
	
	p = _length - 1;
	
	while (p >= s)
	{
		const char ch = _buf[p];
		
		if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
		{
			break;
		}
		
		p --;
	}
	
	return substr(s, p - s + 1);
}

String String::term(size_t &p) const
{
	while (p < _length)
	{
		const char ch = _buf[p];
		
		if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
		{
			break;
		}
		
		p ++;
	}
	
	size_t s = p;
	
	while (p < _length)
	{
		const char ch = _buf[p];
		
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		{
			break;
		}
		
		p ++;
	}
	
	return substr(s, p - s);
}

String String::term(size_t &p, const char delim) const
{
	size_t s = p;
	
	while (p < _length)
	{
		const char ch = _buf[p];
		
		p ++;

		if (ch == delim)
		{
			return substr(s, p - s - 1);
		}		
	}
	
	return substr(s, p - s);
}

String String::line(size_t &p) const
{
	size_t s = p;
	bool eof = false;
	
	while (p < _length)
	{
		const char ch = _buf[p];
		p ++;

		if (ch == '\n')
		{
			eof = false;
			break;
		}
		
		eof = true;
	}
	
	return substr(s, eof ? (p - s) : (p - s - 1));
}

String &String::operator=(const String &m)
{
	if (_buf)
	{
		free(_buf);
	}
	
	if (m._length > 0)
	{
		_length = m._length;
		_capacity = _length + 1;
		_buf = (char*) malloc(_capacity);
		strncpy(_buf, m._buf, _capacity);
	}
	else
	{
		_length = 0;
		_capacity = 0;
		_buf = NULL;
	}
	
	return *this;
}

String &String::operator=(const char* s)
{
	if (_buf)
	{
		free(_buf);
	}
	
	_length = s ? strlen(s) : 0;
	_capacity = _length + 1;
	
	if (_capacity > 1)
	{
		_buf = (char*) malloc(_capacity);
		strncpy(_buf, s, _capacity);
	}
	else
	{
		_capacity = 0;
		_length = 0;
		_buf = NULL;
	}
	
	return *this;
}

String &String::operator+=(const String &m)
{
	size_t ml = m._length;
	
	if (ml + _length >= _capacity)
	{
		unsigned int uml = ml;
		
		if (uml < 4096)
		{
			uml --;
			uml |= uml >> 1;
			uml |= uml >> 2;
			uml |= uml >> 4;
			uml |= uml >> 8;
			uml |= uml >> 16;
			uml ++;
		}

		size_t ts = uml + _length + 1;	
		_buf = (char*) realloc(_buf, ts);
		_capacity = ts;
	}
	
	strncpy(_buf + _length, m._buf, ml + 1);
	_length += ml;
	
	return *this;
}

String &String::operator+=(const char* s)
{
	size_t ml = strlen(s);
	
	if (ml + _length >= _capacity)
	{
		unsigned int uml = ml;
		
		if (uml < 4096)
		{
			uml --;
			uml |= uml >> 1;
			uml |= uml >> 2;
			uml |= uml >> 4;
			uml |= uml >> 8;
			uml |= uml >> 16;
			uml ++;
		}

		size_t ts = uml + _length + 1;	
		_buf = (char*) realloc(_buf, ts);
		_capacity = ts;
	}
	
	strncpy(_buf + _length, s, ml + 1);
	_length += ml;
	
	return *this;
}

String String::operator+(const String &m) const
{
	size_t tl = _length + m._length;
	size_t ts = tl + 1;
	char* tbuf = (char*) malloc(ts);
	
	strncpy(tbuf, _buf, _length + 1);
	strncpy(tbuf + _length, m._buf, m._length + 1);

	return String(tbuf, tl, ts);
}

String String::operator+(const char* s) const
{
	size_t tm = strlen(s);
	size_t tl = _length + tm;
	size_t ts = tl + 1;
	char* tbuf = (char*) malloc(ts);
	
	strncpy(tbuf, _buf, _length + 1);
	strncpy(tbuf + _length, s, tm + 1);

	return String(tbuf, tl, ts);
}

bool String::operator==(const String &m) const
{
	if (_length != m._length)
	{
		return false;
	}
	
	return (strcmp(_buf, m._buf) == 0);
}

bool String::operator==(const char* s) const
{
	if (_length != strlen(s))
	{
		return false;
	}
	
	return (_buf && strcmp(_buf, s) == 0);
}

bool String::operator!=(const String &m) const
{
	if (_length != m._length)
	{
		return true;
	}

	return (strcmp(_buf, m._buf) != 0);
}

bool String::operator!=(const char* s) const
{
	if (_length != strlen(s))
	{
		return true;
	}
	
	return (_buf && strcmp(_buf, s) != 0);
}

char &String::operator[](size_t i)
{
	if (i < _length)
	{
		return _buf[i];
	}
	
	static char _ZERO = 0;
	return _ZERO;
}

int String::toInt() const
{
	return atoi(_buf);
}

float String::toFloat() const
{
	return (float) atof(_buf);
}

String::String(char *buf, size_t len, size_t capacity)
	: _length(len)
	, _capacity(capacity)
	, _buf(buf)
{
}

size_t String::indexOf(const char ch, size_t i) const
{
	if (i < _length)
	{
		for (size_t p = i; p < _length; p ++)
		{
			if (_buf[p] == ch)
				return p;
		}
	}

	return -1;
}

size_t String::lastIndexOf(const char ch, size_t i) const
{
	if (i < 0 || i >= _length)
	{
		i = _length - 1;
	}
	
	for (size_t p = i; p >= 0; p --)
	{
		if (_buf[p] == ch)
			return p;
	}

	return -1;
}

size_t String::indexOf(const String &str, size_t p) const
{
    if (p >= _length)
        return -1;

    const char* pos = strstr(_buf + p, str._buf);

    if (pos)
    {
        return (pos - _buf);
    }
    else
    {
        return -1;
    }
}

String String::replace(const String &str, const String &rep, size_t p) const
{
    const size_t pos = indexOf(str, p);

    if (pos == -1)
        return *this;

    const size_t ppos = pos + rep._length;
    const size_t rlen = _length - str._length + rep._length;
    char* res = (char*) malloc(rlen + 1);

    strncpy(res, _buf, pos);
    strncpy(res + pos, rep._buf, rep._length);
    strncpy(res + ppos, _buf + pos + str._length, _length - pos - str._length + 1);

    return String(res, rlen, rlen + 1);
}

String String::replaceAll(const String &str, const String &rep) const
{
    String sres(*this);
    size_t pos = 0;

    // TODO: Optimize me...
    for (;;)
    {
        pos = sres.indexOf(str, pos);

        if (pos == -1)
            return sres;

        const size_t ppos = pos + rep._length;
        const size_t rlen = sres._length - str._length + rep._length;
        char* res = (char*) malloc(rlen + 1);

        strncpy(res, sres._buf, pos);
        strncpy(res + pos, rep._buf, rep._length);
        strncpy(res + ppos, sres._buf + pos + str._length, sres._length - pos - str._length + 1);

        free(sres._buf);
        sres._buf = res;
        sres._length = rlen;
        sres._capacity = rlen + 1;
    }
}

String String::toLowerCase() const
{
    char* buf = (char*) malloc(_length + 1);

    for (int i = 0; i <= _length; i ++)
    {
        char ch = _buf[i];

        if (ch >= 'A' && ch <= 'Z')
            ch = ch - 'A' + 'a';

        buf[i] = ch;
    }

    return String(buf, _length, _length + 1);
}

String String::toUpperCase() const
{
    char* buf = (char*) malloc(_length + 1);

    for (int i = 0; i <= _length; i ++)
    {
        char ch = _buf[i];

        if (ch >= 'a' && ch <= 'z')
            ch = ch - 'a' + 'A';

        buf[i] = ch;
    }

    return String(buf, _length, _length + 1);
}
