#pragma once

#include <stdint.h>
#include "ustring.h"
#include "array.h"

enum EXIFTAG_TYPE
{
	EXIFTAG_UNKNOWN = 0,
	EXIFTAG_UBYTE = 1,
	EXIFTAG_ASCII = 2,
	EXIFTAG_USHORT = 3,
	EXIFTAG_ULONG = 4,
	EXIFTAG_URATIONAL = 5,
	EXIFTAG_BYTE = 6,
	EXIFTAG_UNDEFINED = 7,
	EXIFTAG_SHORT = 8,
	EXIFTAG_LONG = 9,
	EXIFTAG_RATIONAL = 10,
	EXIFTAG_FLOAT = 11,
	EXIFTAG_DOUBLE = 12
};

class ExifTag
{
public:
	uint16_t tag;
	uint16_t type;
	uint32_t size;
	void*	 data;
	
	explicit ExifTag(uint16_t _tag);
	~ExifTag();
    ExifTag* clone() const;

	void set(uint16_t t, uint32_t s, const void* v);
	
	void setData(size_t l, const void* v);
	
	void setString(const String &v);
	void setInt16(const int16_t v);
	void setUInt16(const uint16_t v);
	void setInt32(const int32_t v);
	void setUInt32(const uint32_t v);
	void setRational(const int32_t* v);
	void setURational(const uint32_t* v);
	
	size_t getPayLoadSize() const;
};

class ExifIfd
{
public:
	void	    addTag(ExifTag* tag);
    ExifTag*    addTag(uint16_t tag);
	void	    setNextIfd(uint32_t p);
	size_t	    getSize() const;
	void	    save(void* buf, size_t offset) const;
    size_t      find(uint16_t tag) const;
    ExifTag*    at(size_t id);
    const ExifTag*    at(size_t id) const;
	
	ExifIfd();
	~ExifIfd();
	
private:
	static void	emitChar(void* buf, size_t &p, char v);
	static void	emitShort(void* buf, size_t &p, uint16_t v);
	static void	emitLong(void* buf, size_t &p, uint32_t v);
	static void	emitData(void* buf, size_t &p, size_t l, const void* v);

	Array<ExifTag*>		_tags;
	uint32_t			_nextIfd;
};
