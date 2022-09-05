#include "exif.h"

static uint32_t TAGBYTE[] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};

ExifTag::ExifTag(uint16_t _tag)
	: tag(_tag)
	, type(0)
	, size(1)
	, data(0)
{
}

ExifTag::~ExifTag()
{
	if (data)
		free (data);
}

ExifTag* ExifTag::clone() const
{
    ExifTag* t = new ExifTag(tag);
    t->set(type, size, data);
    return t;
}

void ExifTag::set(uint16_t t, uint32_t s, const void* v)
{
	if (data)
		free (data);
		
	type = t;
	size = s;
	
	size_t ps = size * TAGBYTE[type];
	if (ps < 4) ps = 4;
	
	data = malloc(ps);
    if (ps == 4)
        memset(data, 0, ps);

	memcpy(data, v, ps);
}

void ExifTag::setData(size_t l, const void* v)
{
	set(EXIFTAG_UNDEFINED, l, v);
}

void ExifTag::setString(const String &v)
{
	set(EXIFTAG_ASCII, v.length() + 1, v.c_str());
}

void ExifTag::setInt16(const int16_t v)
{
	set(EXIFTAG_SHORT, 1, &v);
}

void ExifTag::setUInt16(const uint16_t v)
{
	set(EXIFTAG_USHORT, 1, &v);
}

void ExifTag::setInt32(const int32_t v)
{
	set(EXIFTAG_LONG, 1, &v);
}

void ExifTag::setUInt32(const uint32_t v)
{
	set(EXIFTAG_ULONG, 1, &v);
}

void ExifTag::setRational(const int32_t* v)
{
	set(EXIFTAG_RATIONAL, 1, v);
}

void ExifTag::setURational(const uint32_t* v)
{
	set(EXIFTAG_URATIONAL, 1, v);
}

size_t ExifTag::getPayLoadSize() const
{
	uint32_t dsize = size * TAGBYTE[type];
	return (dsize <= 4) ? 0 : dsize;
}


void ExifIfd::addTag(ExifTag* tag)
{
	_tags.add(tag);
}

ExifTag* ExifIfd::addTag(uint16_t tag)
{
    ExifTag* t = new ExifTag(tag);
	_tags.add(t);
    return t;
}

void ExifIfd::setNextIfd(uint32_t p)
{
	_nextIfd = p;
}

size_t ExifIfd::getSize() const
{
	size_t ifdlen = 2 + _tags.size() * 12 + 4;
	const int c = _tags.size();
		
	for (int i = 0; i < c; i ++)
	{
		ifdlen += _tags[i]->getPayLoadSize();
	}
	
	return ifdlen;
}

void ExifIfd::save(void* buf, size_t offset) const
{
	size_t pos = 0;
	size_t ifdlen = 2 + _tags.size() * 12 + 4;
	const int c = _tags.size();
	
	emitShort(buf, pos, c);

	// Pass 1, tags
	for (int i = 0; i < c; i ++)
	{
		ExifTag* tag = _tags[i];
		
		emitShort(buf, pos, tag->tag);
		emitShort(buf, pos, tag->type);
		emitLong(buf, pos, tag->size);
	
		size_t psize = tag->getPayLoadSize();
		
		if (psize == 0)
		{
			emitData(buf, pos, 4, tag->data);
		}
		else
		{
			emitLong(buf, pos, ifdlen + offset);
			ifdlen += psize;
		}
	}

	emitLong(buf, pos, _nextIfd);
	
	// Pass 2, payloads
	for (int i = 0; i < c; i ++)
	{
		ExifTag* tag = _tags[i];		
		size_t psize = tag->getPayLoadSize();		
		if (psize > 0)
		{
			emitData(buf, pos, psize, tag->data);
		}
	}	
}

ExifIfd::ExifIfd()
	: _nextIfd(0)
{
}

ExifIfd::~ExifIfd()
{
	const int c = _tags.size();
	for (int i = 0; i < c; i ++)
	{
		delete _tags[i];
	}
}

ExifTag* ExifIfd::at(size_t id)
{
    if (id < _tags.size())
        return _tags[id];

    return NULL;
}

const ExifTag* ExifIfd::at(size_t id) const
{
    if (id < _tags.size())
        return _tags[id];

    return NULL;
}

size_t ExifIfd::find(uint16_t tag) const
{
    const size_t c = _tags.size();
	for (size_t i = 0; i < c; i ++)
	{
        if (_tags[i]->tag == tag)
            return i;
    }

    return -1;
}

void ExifIfd::emitChar(void* buf, size_t &p, char v)
{
	*((uint8_t*) buf + p ++) = v;
}

void ExifIfd::emitShort(void* buf, size_t &p, uint16_t v)
{
	*((uint8_t*) buf + p ++) = (v & 0xFF);
	*((uint8_t*) buf + p ++) = ((v >> 8) & 0xFF);
}

void ExifIfd::emitLong(void* buf, size_t &p, uint32_t v)
{
	*((uint8_t*) buf + p ++) = (v & 0xFF);
	*((uint8_t*) buf + p ++) = ((v >> 8) & 0xFF);
	*((uint8_t*) buf + p ++) = ((v >> 16) & 0xFF);
	*((uint8_t*) buf + p ++) = ((v >> 24) & 0xFF);
}

void ExifIfd::emitData(void* buf, size_t &p, size_t l, const void* v)
{
	memcpy((uint8_t*) buf + p, v, l);	
	p += l;
}

