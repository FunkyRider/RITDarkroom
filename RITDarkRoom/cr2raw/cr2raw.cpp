#include <time.h>
#include <math.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#ifdef WIN32
#include <sys/utime.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include "cr2raw.h"

static uint32_t TAGBYTE[] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};

static void _swab(const void *bfrom, void *bto, size_t n)
{
	const char *from = (const char *) bfrom;
	char *to = (char *) bto;

	n &= ~((size_t) 1);
	while (n > 1)
    {
		const char b0 = from[--n], b1 = from[--n];
		to[n] = b0;
		to[n + 1] = b1;
    }
}

CR2Exception::CR2Exception(const char* file, int line, const CR2ExceptionCode code)
    : _file(file)
    , _line(line)
    , _code(code)
{
}

void CR2Exception::print()
{
    int32_t psize = strlen(_file);
	int32_t i = 0;

	for (i = psize; i > 0; i --)
	{
		if (_file[i] == '\\' || _file[i] == '/')
			break;
    }

    printf("Error: %s(%d): %s\n", _file + i + 1, _line, getMessage());
}

const CR2ExceptionCode CR2Exception::getCode()
{
    return _code;
}

const char* CR2Exception::getMessage()
{
     static const char* _MSGTEXT[] = {
        "Unknown error",
        "Out of memory",
        "Input file not found",
        "Cannot create output file",
        "Input file read error",
        "Output file write error",
        "Input file format error",
        "Output file format error"
    };

    return _MSGTEXT[_code];
}

CR2Raw::CR2Raw(const char* fname)
{    
    memset(this, 0, sizeof (CR2Raw));    
    size_t flen = strlen(fname);    
    _fileName = new char[flen + 1];    
    strcpy(_fileName, fname);
    
    _ifp = _fopen(_fileName, "rb");
    if (!_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);

    parseTiff(0);
    
    if (_embedJpegOffset > 0)
    {
    	_rawFormat = CR2RF_PanasonicRW2;
    	// Panasonic additional tags are in embedded JPG
    	parseTiff(_embedJpegOffset + 12);
    }
    
    // Determine RAW data IFD (3)
    for (uint32_t i = 0; i < _ifdCount; i++)
    {
        if ((_ifd[i].comp != 6 || _ifd[i].samples != 3) &&
           (_ifd[i].width | _ifd[i].height) < 0x10000 &&
            _ifd[i].width * _ifd[i].height > (int32_t)(_rawWidth * _rawHeight))
        {
            _rawWidth    = _ifd[i].width;
            _rawHeight   = _ifd[i].height;
            _rawBps      = _ifd[i].bps;
            _rawCompress = _ifd[i].comp;
            _rawOffset   = _ifd[i].offset;
            _rawFlip     = _ifd[i].flip;
            _rawIfd	= i;            
            _rawSizeTagPos		= _ifd[i].bytesTagPos;
        }
    }

    if (_serialNumber2[0] == 0 && _serialNumber != 0)
    {
        sprintf(_serialNumber2, "%u", _serialNumber);
    }

    switch (_modelID)
    {
    case 0x80000174:    // 1D2
    case 0x80000188:    // 1Ds2
    case 0x80000232:    // 1D2N
    case 0x80000213:    // 5D
        _lensID = ((uint32_t) (_camInfo[12])) << 8 | _camInfo[13];
        break;
    case 0x80000169:    // 1D3
    case 0x80000215:    // 1Ds3
        _lensID = ((uint32_t) (_camInfo[273])) << 8 | _camInfo[274];
        break;
    case 0x80000281:    // 1D4
        _lensID = ((uint32_t) (_camInfo[335])) << 8 | _camInfo[336];
        break;
    case 0x80000269:    // 1DX
    case 0x80000324:    // 1DC
        _lensID = ((uint32_t) (_camInfo[423])) << 8 | _camInfo[424];
        break;
    case 0x80000218:    // 5D2
        _lensID = ((uint32_t) (_camInfo[230])) << 8 | _camInfo[231];
        break;
    case 0x80000285:    // 5D3
        _lensID = ((uint32_t) (_camInfo[339])) << 8 | _camInfo[340];
        break;
    case 0x80000302:    // 6D
        _lensID = ((uint32_t) (_camInfo[353])) << 8 | _camInfo[354];
        break;
    }

    if (_sensorInfo[1] == 0 || _sensorInfo[2] == 0)
    {
        _sensorInfo[0] = 0;
        _sensorInfo[1] = _rawWidth;
        _sensorInfo[2] = _rawHeight;
        _sensorInfo[3] = _sensorInfo[4] = 1;
        _sensorInfo[5] = _cropLeft;
        _sensorInfo[6] = _cropTop;
        _sensorInfo[7] = _cropRight - 1;
        _sensorInfo[8] = _cropBottom - 1;
        
        _exifWidth = _sensorInfo[7] - _sensorInfo[5] + 1;
        _exifHeight = _sensorInfo[8] - _sensorInfo[6] + 1;
        _aspectInfo[1] = _exifWidth;
        _aspectInfo[2] = _exifHeight;
    }

    if (_rawBps == 0)
        _rawBps = 12;


    // FORC(16) printf("%d ", _sensorInfo[c]);
	/*FORC(_customFunctionsCount) {	
		printf("%d (%04x)\n", _customFunctions[c], _customFunctions[c]);
	}
	*/
    
    /*
    FORC(4)
    {
        TiffIfd &ifd = _ifd[c];

        printf("IFD #%d:\n", c);
        printf("Start:\t%d\n", ifd.base);
        printf("Length:\t%d\n", ((c == 3) ? _endOfIfd : ifd.nextIfd) - ifd.base);
        printf("W: %d, H: %d, Bps: %d\n", ifd.width, ifd.height, ifd.bps);
        printf("Data Start:\t%d\n", ifd.offset);
        printf("Data Length:\t%d\n", ifd.bytes);
    }
    */
    
    fclose(_ifp);
    _ifp = NULL;
}

// Rearrange old style CR2 (1Ds2) where metadata and image are interleaved
// to place metadata in front and image at the end (modern style)
void CR2Raw::reformat(const char* fname)
{
	_ifp = _fopen(_fileName, "rb");
    if (!_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
    
    FILE* ofp = _fopen(fname, "wb");

    // write tiff header
    fseek(_ifp, 0, SEEK_SET);
    copyBytes(ofp, _ifp, 16);

    int32_t newbase[4] = {0, 0, 0, 0};
    int32_t newnext[4] = {0, 0, 0, 0};
    int32_t newoffset[4] = {0, 0, 0, 0};
    int32_t appendix[4] = {16, 0, 6, 6};

    // write IFDs
    FORC(4)
    {
        TiffIfd &ifd = _ifd[c];
        fseek(_ifp, ifd.base, SEEK_SET);
        size_t len = ifd.offset - ifd.base;

        if (c > 0) newnext[c - 1] = ftell(ofp);
        newbase[c] = ftell(ofp);
        copyBytes(ofp, _ifp, len + appendix[c]);
    }

    FORC(1024) fputc(0, ofp);

    // write images
    FORC(4)
    {
        TiffIfd &ifd = _ifd[c];
        fseek(_ifp, ifd.offset, SEEK_SET);
        newoffset[c] = ftell(ofp);
        copyBytes(ofp, _ifp, c == 3 ? 4096 : ifd.bytes);
    }

    // re-write IFDs with updated offset and nextIfd tags
    FORC(4)
    {
        TiffIfd &ifd = _ifd[c];

        fseek(ofp, ifd.nextIfdTagPos + newbase[c] - ifd.base, SEEK_SET);
        put4(newnext[c], ofp);

        fseek(ofp, ifd.offsetTagPos + newbase[c] - ifd.base, SEEK_SET);
        put4(newoffset[c], ofp);
    }

    fclose(ofp);
    
    fclose(_ifp);
    _ifp = NULL;
}

ExifIfd &CR2Raw::getExif()
{
	return _exif;
}

CR2Raw::~CR2Raw()
{
    if (_ifp)
        fclose(_ifp);

    if (_rawImage)
        free(_rawImage);
        
    if (_vrd)
    	free(_vrd);

    if (_camInfo)
        free(_camInfo);
        
    for (uint32_t i = 0; i < _ifdCount; i ++)
    {
    	if (_ifd[i].payload)
    		free(_ifd[i].payload);
    }

    delete [] _fileName;
}

uint16_t* CR2Raw::createRawData(int32_t w, int32_t h, uint16_t black)
{
	if (_rawImage)
		delete _rawImage;
	
	_rawImage = (uint16_t*) malloc(w * h * 2);
	
	_rawWidth = w;
	_rawHeight = h;

    _ifd[_rawIfd].width = w;
    _ifd[_rawIfd].height = h;
	
	const int32_t s = w * h;
	
	for (int32_t i = 0; i < s; i ++)
	{
		_rawImage[i] = black;
	}
	
	return _rawImage;
}

uint16_t* CR2Raw::getRawData()
{
	_ifp = _fopen(_fileName, "rb");
    if (!_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
    
    if (!_rawImage)
    {
        _rawImage = (uint16_t*) calloc(_rawWidth, _rawHeight * 2);
        fseek(_ifp, _rawOffset, SEEK_SET);

        switch (_rawFormat)
        {
        case CR2RF_CanonCR2:
            {
                LJpegDecoder decoder(_ifp);
                decoder.getStream(_rawImage, _cr2Slice);
            }
            break;
        case CR2RF_PanasonicRW2:
            {
                RW2Decoder decoder(_ifp);
                decoder.getStream(_rawImage, _rawWidth, _rawHeight, _cropRight);
            }
            break;
        default:
            throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);
        }
    }
    
    if (!_vrd && _vrdPos)
    {
	    parseVRD(_vrdPos);
    }

	fclose(_ifp);
    _ifp = NULL;
    
    return _rawImage;
}

const int32_t CR2Raw::getWidth() const
{
	return _rawWidth;
}

const int32_t CR2Raw::getHeight() const
{
	return _rawHeight;
}

const uint32_t CR2Raw::getCfaType() const
{
	return _cfaPattern;
}

uint32_t CR2Raw::getModelID() const
{
    return _modelID;
}

uint16_t CR2Raw::getLensID() const
{
    return _lensID;
}

const char* CR2Raw::getModel() const
{
	return _model;
}

const char* CR2Raw::getSerialNumber() const
{
    return _serialNumber2;
}

const char* CR2Raw::getLensModel() const
{
    return _lensName;
}

int32_t CR2Raw::getFocalLength() const
{
    return (int32_t) _focalLength;
}

size_t CR2Raw::getCommentLen() const
{
	return _commentLen;
}

char* CR2Raw::getComment()
{
	return _comment;
}

const uint16_t CR2Raw::getBps() const
{
    return _rawBps;
}

const int32_t CR2Raw::getISO() const
{
	return (int32_t) _isoSpeed;
}

uint32_t* CR2Raw::getWBCoefficients()
{
    return _wb;
}

uint32_t* CR2Raw::getWBTemperature()
{
	return _wbtemp;
}

uint16_t* CR2Raw::getSliceData()
{
	return _cr2Slice;
}

uint16_t* CR2Raw::getSensorInfo()
{
	return _sensorInfo;
}

uint32_t* CR2Raw::getRW2BlackSatWB()
{
	return _rw2blacksatwb;
}

CR2RawFormat CR2Raw::getRawFormat() const
{
	return _rawFormat;
}

void CR2Raw::copyExif(const CR2Raw* src)
{
	static int32_t CAMSET_POS[] = {1, 2, 4, 5, 7, 9, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 41, 42};
	static int32_t SHOTINFO_POS[] = {3, 4, 5, 6, 9, 15, 16, 17, 21, 22, 23, 24, 26, 27};
	
	strncpy(_model, src->_model, COUNT(_model));
	strncpy(_desc, src->_desc, COUNT(_desc));
	strncpy(_lensName, src->_lensName, COUNT(_lensName));
	strncpy(_artist, src->_artist, COUNT(_artist));
	memcpy(_szTimeStamp, src->_szTimeStamp, 20);

	_exposureMode = src->_exposureMode;
	_isoSpeed = src->_isoSpeed;
	_shutter = src->_shutter;
	_aperture = src->_aperture;
	_focalLength = src->_focalLength;
	_meteringMode = src->_meteringMode;
	_timestamp = src->_timestamp;
	
	_shutterReal[0] = src->_shutterReal[0];
	_shutterReal[1] = src->_shutterReal[1];
	_apertureReal[0] = src->_apertureReal[0];
	_apertureReal[1] = src->_apertureReal[1];
	_focalLengthReal[0] = src->_focalLengthReal[0];
	_focalLengthReal[1] = src->_focalLengthReal[1];
    _shutter2[0] = src->_shutter2[0];
    _shutter2[1] = src->_shutter2[1];
    _aperture2[0] = src->_aperture2[0];
    _aperture2[1] = src->_aperture2[1];
	_exposureComp[0] = src->_exposureComp[0];
	_exposureComp[1] = src->_exposureComp[1];
    _orientation = src->_orientation;

	if (src->_rawFormat != CR2RF_CanonCR2)
		return;

    for (uint32_t i = 0; i < COUNT(CAMSET_POS); i ++)
	    _cameraSettings[CAMSET_POS[i]] = src->_cameraSettings[CAMSET_POS[i]];

    for (uint32_t i = 0; i < COUNT(SHOTINFO_POS); i ++)
	    _shotInfo[SHOTINFO_POS[i]] = src->_shotInfo[SHOTINFO_POS[i]];
    

    memcpy(_serialNumber2, src->_serialNumber2, COUNT(_serialNumber2));
    _serialNumber2Len = src->_serialNumber2Len;
    if (src->_serialNumberTagPos > 0)
    {
	    _serialNumber = src->_serialNumber;
    }
    else if (src->_serialNumber2TagPos > 0)
    {
        uint32_t i = 0;
        uint32_t s = 0;

        while ((s = atoi(_serialNumber2 + i)) > 0x7fffffff && i < _serialNumber2Len)
        {
            i ++;
        }

        if (i < _serialNumber2Len)
        {
            _serialNumber = s;
        }
    }
    else
    {
        _serialNumber = 0;
    }

	_imageNumber = src->_imageNumber;

    _resX[0] = src->_resX[0];
    _resX[1] = src->_resX[1];
    _resY[0] = src->_resY[0];
    _resY[1] = src->_resY[1];
    _resUnit = src->_resUnit;
	
    FORC(8) _wb[c] = src->_wb[c];
    FORC2 _wbtemp[c] = src->_wbtemp[c];
    
    _exifWidth = src->_exifWidth;
    _exifHeight = src->_exifHeight;
    FORC(5) _aspectInfo[c] = src->_aspectInfo[c];
}

void CR2Raw::setModelID(uint32_t modelID)
{
	_modelID = modelID;
}

void CR2Raw::save(const char* fname, CR2Raw* thumbs, StreamReader* fljpeg)
{
	_ifp = _fopen(_fileName, "rb");
    if (!_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
    
    if (thumbs)
    {
		thumbs->_ifp = _fopen(thumbs->_fileName, "rb");
		if (!thumbs->_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
	}
	
	FILE* ofp = _fopen(fname, "wb");
	if (!ofp) throw CR2Exception(__FILE__, __LINE__, CR2E_CannotCreateOutputFile);

    // Thumbs can be from another raw file
    if (thumbs == NULL) thumbs = this;

	if (thumbs == this)
	{
		fseek(_ifp, 0, SEEK_SET);
		copyBytes(ofp, _ifp, _ifd[_rawIfd].offset);
	}
    else if (thumbs->_rawFormat == CR2RF_PanasonicRW2)
    {
        // Copy original IFD sections to new till _endOfIfd
		fseek(_ifp, 0, SEEK_SET);
		copyBytes(ofp, _ifp, _endOfIfd + 12);

        // Write IFD0...IFD2 payload (thumbnail, embed JPG, uncompressed RGB)
        // Embedded JPG
        _ifd[0].width = 1920;
        _ifd[0].height = 1440;
        _ifd[0].offset = ftell(ofp);
        _ifd[0].bytes = thumbs->_embedJpegSize;
        fseek(thumbs->_ifp, thumbs->_embedJpegOffset, SEEK_SET);
        copyBytes(ofp, thumbs->_ifp, _ifd[0].bytes);

        // Thumbnail
        _ifd[1].width = thumbs->_ifd[2].width;
        _ifd[1].height = thumbs->_ifd[2].height;
        _ifd[1].offset = ftell(ofp);
        _ifd[1].bytes = thumbs->_ifd[2].bytes;
        fseek(thumbs->_ifp, thumbs->_ifd[2].offset, SEEK_SET);
        copyBytes(ofp, thumbs->_ifp, _ifd[1].bytes);
   
        // Uncompressed RGB
        const int32_t bytes = (_rawBps > 12) ? 2 : 1;
	    uint8_t* rgb = getSensorRGBPreview(_ifd[2].width, _ifd[2].height, bytes * 8);
        _ifd[2].offset = ftell(ofp);
	    writeBytes(ofp, rgb, _ifd[2].width * _ifd[2].height * 3 * bytes);
	    free(rgb);
    }
	else if (thumbs->_rawFormat == CR2RF_CanonCR2)
	{
		// Copy original IFD sections to new till _endOfIfd
		fseek(_ifp, 0, SEEK_SET);
		copyBytes(ofp, _ifp, _endOfIfd + 12);
		
		// Write IFD0...IFD2 payload (thumbnail, embed JPG, uncompressed RGB)
		const int32_t payloadOrder[] = {1, 0, 2};
		FORC3
		{
		    const CR2Raw* from = (payloadOrder[c] == 1 || payloadOrder[c] == 0) ? thumbs : this;
			
		    TiffIfd &sifd = _ifd[payloadOrder[c]];
		    const TiffIfd &difd = from->_ifd[payloadOrder[c]];
		    const size_t o = difd.offset;

		    sifd.width = difd.width;
		    sifd.height = difd.height;
		    sifd.bytes = difd.bytes;
		    sifd.offset = ftell(ofp);

		    if (payloadOrder[c] == 2)
		    {
		        // Generate sensor preview and write to output file
                const int32_t bytes = (_rawBps > 12) ? 2 : 1;

                if (fljpeg == NULL)
                {
		            uint8_t* rgb = getSensorRGBPreview(sifd.width, sifd.height, bytes * 8);
		            writeBytes(ofp, rgb, sifd.width * sifd.height * 3 * bytes);
		            free(rgb);
                }
                else
                {
                    // If saving CR2 using outside LJpeg stream, cannot get a sensor preview
                    // so instead save a black output
                    uint8_t* pline = (uint8_t*) malloc(sifd.width * 3 * bytes);
                    memset(pline, 0, sifd.width * 3 * bytes);
                    FORC(sifd.height) writeBytes(ofp, pline, sifd.width * 3 * bytes);
		            free(pline);
                }
		    }
		    else
		    {
		        // Straignt copy of thumbnail and embedded JPG
		    	fseek(from->_ifp, o, SEEK_SET);
		        copyBytes(ofp, from->_ifp, sifd.bytes);
		    }
		}
	}

	// Write IFD 3 payload (RAW data in LJpeg);
    {
        TiffIfd &sifd = _ifd[_rawIfd];

		_rawHeader.wide = _rawWidth / 2;
		_rawHeader.high = _rawHeight;

        sifd.width = _rawWidth;
        sifd.height = _rawHeight;
        sifd.offset = ftell(ofp);

        if (fljpeg == NULL)
        {
		    LJpegEncoder encoder(ofp);
		    encoder.setInfo(_rawHeader);
		    encoder.setStream(_rawImage, _cr2Slice);
        }
        else
        {
			char buf[4096];
			size_t len = 0;

			while ((len = fljpeg->read(fljpeg->ptr, buf, 4096)) > 0)
			{
				fwrite(buf, 1, len, ofp);
			}
        }

		sifd.bytes = ftell(ofp) - sifd.offset;
	}
	
	// (Optional) Save VRD block
	if (_vrdPos > 0)
	{
        if (!_vrd)
        {
            parseVRD(_vrdPos);
        }

		size_t newVrdPos = ftell(ofp);
		if (fwrite(_vrd, _vrdLen, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		fseek(ofp, _vrdTagPos, SEEK_SET);
		put4(newVrdPos, ofp);
        _vrdPos = newVrdPos;
	}
	
	// Update IFD payload tags
    FORC4
    {
        const TiffIfd &sifd = _ifd[c];

        if (sifd.widthTagPos)
        {
            fseek(ofp, sifd.widthTagPos, SEEK_SET);
            put2(sifd.width, ofp);
        }

        if (sifd.heightTagPos)
        {
            fseek(ofp, sifd.heightTagPos, SEEK_SET);
            put2(sifd.height, ofp);
        }

        if (sifd.offsetTagPos)
        {
            fseek(ofp, sifd.offsetTagPos, SEEK_SET);
            put4(sifd.offset, ofp);
        }

        if (sifd.bytesTagPos)
        {
            fseek(ofp, sifd.bytesTagPos, SEEK_SET);
            put4(sifd.bytes, ofp);
        }
    }

	// Update EXIF tags
	if (_modelIDTagPos > 0)
	{
		fseek(ofp, _modelIDTagPos, SEEK_SET);
		put4(_modelID, ofp);
	}
	
	if (_modelTagPos > 0)
	{
		fseek(ofp, _modelTagPos, SEEK_SET);
		if (fwrite(_model, 64, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
	
	if (_descTagPos > 0)
	{
		fseek(ofp, _descTagPos, SEEK_SET);
		if (fwrite(_desc, 512, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
	
	if (_lensNameTagPos > 0)
	{
		fseek(ofp, _lensNameTagPos, SEEK_SET);
		if (fwrite(_lensName, _lensNameTagLen, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
	
	if (_artistTagPos > 0)
	{
		fseek(ofp, _artistTagPos, SEEK_SET);
		if (fwrite(_artist, 64, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
	
	if (_cameraSettingsTagPos > 0)
	{
		fseek(ofp, _cameraSettingsTagPos, SEEK_SET);
		FORC(_cameraSettingsCount) put2(_cameraSettings[c], ofp);
	}
	
	if (_shotInfoTagPos > 0)
	{
		fseek(ofp, _shotInfoTagPos, SEEK_SET);
		FORC(_shotInfoCount) put2(_shotInfo[c], ofp);
	}
	
	if (_exposureModeTagPos > 0)
	{
		fseek(ofp, _exposureModeTagPos, SEEK_SET);
		put2(_exposureMode, ofp);
	}
	
	if (_shutterTagPos > 0)
	{
		fseek(ofp, _shutterTagPos, SEEK_SET);
		FORC2 put4(_shutterReal[c], ofp);
	}
	
	if (_apertureTagPos > 0)
	{
		fseek(ofp, _apertureTagPos, SEEK_SET);
		FORC2 put4(_apertureReal[c], ofp);
	}

	if (_shutterTagPos2 > 0)
	{
		fseek(ofp, _shutterTagPos2, SEEK_SET);
		FORC2 put4(_shutter2[c], ofp);
	}
	
	if (_apertureTagPos2 > 0)
	{
		fseek(ofp, _apertureTagPos2, SEEK_SET);
		FORC2 put4(_aperture2[c], ofp);
	}

	if (_exposureCompTagPos > 0)
	{
		fseek(ofp, _exposureCompTagPos, SEEK_SET);
		FORC2 put4(_exposureComp[c], ofp);
	}
	
	if (_focalLengthTagPos > 0)
	{
		fseek(ofp, _focalLengthTagPos, SEEK_SET);
		FORC2 put4(_focalLengthReal[c], ofp);
	}
	
	if (_focalLengthMakerNoteTagPos > 0)
	{
		fseek(ofp, _focalLengthMakerNoteTagPos, SEEK_SET);
		FORC4 put2(_focalLengthMakerNote[c], ofp);
	}
	
	if (_isoSpeedTagPos > 0)
	{
		fseek(ofp, _isoSpeedTagPos, SEEK_SET);
		put2((uint16_t) _isoSpeed, ofp);
	}
	
	if (_meteringModeTagPos > 0)
	{
		fseek(ofp, _meteringModeTagPos, SEEK_SET);
		put2((uint16_t) _meteringMode, ofp);	
	}
	
	if (_serialNumberTagPos > 0)
	{
		fseek(ofp, _serialNumberTagPos, SEEK_SET);
		put4(_serialNumber, ofp);
	}
	
	
	for (int i = 0; i < 8; i ++)
	if (_wbTagPos[i] > 0)
	{
		fseek(ofp, _wbTagPos[i], SEEK_SET);
		FORC4 put2(_wb[c + i * 4], ofp);
		
		if (i == 0)
		{
			fseek(ofp, 2, SEEK_CUR);
			FORC4 put2(_wb[c + i * 4], ofp);
		}
	}
	
	
	if (_cr2SliceTagPos > 0)
	{
		fseek(ofp, _cr2SliceTagPos, SEEK_SET);
		FORC3 put2(_cr2Slice[c], ofp);
	}
	
	if (_sensorInfoTagPos > 0)
	{
		fseek(ofp, _sensorInfoTagPos, SEEK_SET);
		FORC(17) put2(_sensorInfo[c], ofp);

        _exifWidth = _sensorInfo[7] - _sensorInfo[5] + 1;
        _exifHeight = _sensorInfo[8] - _sensorInfo[6] + 1;
	}
	
	if (_aspectInfoTagPos > 0)
	{
		fseek(ofp, _aspectInfoTagPos, SEEK_SET);
		FORC(5) put4(_aspectInfo[c], ofp);
	}
	
	// EXIF width/height
	if (_exifWidthTagPos > 0)
	{
		fseek(ofp, _exifWidthTagPos, SEEK_SET);
        put2(_exifWidth, ofp);
	}
	
	if (_exifHeightTagPos > 0)
	{
		fseek(ofp, _exifHeightTagPos, SEEK_SET);
        put2(_exifHeight, ofp);
	}

    if (_resXTagPos > 0)
    {
        fseek(ofp, _resXTagPos, SEEK_SET);
        FORC2 put4(_resX[c], ofp);
    }
	
    if (_resYTagPos > 0)
    {
        fseek(ofp, _resYTagPos, SEEK_SET);
        FORC2 put4(_resY[c], ofp);
    }

    if (_resUnitTagPos > 0)
    {
        fseek(ofp, _resUnitTagPos, SEEK_SET);
        put2(_resUnit, ofp);
    }

    if (_orientationTagPos > 0)
    {
        fseek(ofp, _orientationTagPos, SEEK_SET);
        put2(_orientation, ofp);
    }
    
    if (_customFunctionsTagPos > 0)
    {
    	fseek(ofp, _customFunctionsTagPos, SEEK_SET);
    	FORC(_customFunctionsCount) put4(_customFunctions[c], ofp);
    }
    
    if (_commentTagPos > 0)
    {
    	fseek(ofp, _commentTagPos, SEEK_SET);
    	if (fwrite(_comment, 1, _commentLen, ofp) < _commentLen) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
    }

    FORC(_timestampCount)
	{
		fseek(ofp, _timestampTagPos[c], SEEK_SET);
		if (fwrite(_szTimeStamp, 20, 1, ofp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
	
	fclose(ofp);
	
	if (thumbs && thumbs != this)
	{
		fclose(thumbs->_ifp);
		thumbs->_ifp = NULL;
	}
		
	fclose(_ifp);
    _ifp = NULL;
}

void CR2Raw::loadVRD(const char* fname)
{
    CR2Raw raw(fname);

    if (raw._vrdPos > 0)
    {
        if (!raw._vrd)
        {
            raw._ifp = _fopen(raw._fileName, "rb");
		    if (!raw._ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);	    
	        raw.parseVRD(raw._vrdPos);		
		    fclose(raw._ifp);
		    raw._ifp = NULL;
        }

        if (_vrd)
            free(_vrd);

        _vrd = raw._vrd;
        _vrdLen = raw._vrdLen;
        _vrdPos = 1;
        raw._vrd = 0;
    }
}

bool CR2Raw::saveVRD(const char* fname, size_t* pos, size_t* len)
{
	if (!_vrdPos)
		return false;

    if (!_vrd)
    {
		_ifp = _fopen(_fileName, "rb");
		if (!_ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
	    
	    parseVRD(_vrdPos);
		
		fclose(_ifp);
		_ifp = NULL;
	}
	
	CR2Raw dest(fname);
	
	dest._ifp = _fopen(dest._fileName, "rb+");
	if (!dest._ifp) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileNotFound);
	
	fseek(dest._ifp, dest._vrdTagPos, SEEK_SET);
	size_t newVrdPos = dest.get4();
	
	if (newVrdPos == 0)
	{
		fseek(dest._ifp, 0, SEEK_END);
		newVrdPos = ftell(dest._ifp);
	}
	else
	{
		fseek(dest._ifp, newVrdPos, SEEK_SET);
	}
	
	if (fwrite(_vrd, 1, _vrdLen, dest._ifp) < _vrdLen) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	
	if (pos)
		*pos = newVrdPos;
	if (len)
		*len = _vrdLen;
	
	fseek(dest._ifp, dest._vrdTagPos, SEEK_SET);
	put4(newVrdPos, dest._ifp);
	
	fclose(dest._ifp);
	dest._ifp = NULL;
	
	return true;
}

void CR2Raw::saveRawData(const char* fname)
{
    FILE* ofp = _fopen(fname, "wb+");

    if (!ofp) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);

	_rawHeader.wide = _rawWidth / 2;
	_rawHeader.high = _rawHeight;

	LJpegEncoder encoder(ofp);
	encoder.setInfo(_rawHeader);
	encoder.setStream(_rawImage, _cr2Slice);

	fclose(ofp);
}

void CR2Raw::tiffGet(size_t base, uint32_t* tag, uint32_t* type, uint32_t* len, uint32_t* save)
{
    *tag  = get2();
    *type = get2();
    *len  = get4();
    *save = ftell(_ifp) + 4;

    if (*len * ("11124811248484"[*type < 14 ? *type : 0] - '0') > 4)
        fseek(_ifp, get4() + base, SEEK_SET);
}

int32_t CR2Raw::parseTiffIfd(size_t base)
{
    uint32_t entries, tag, type, len, save;
    uint32_t ifd, i;
	
    size_t off = ftell(_ifp);

    if (_ifdCount >= COUNT(_ifd))
        return 1;

    ifd = _ifdCount++;
    entries = get2();

    if (entries > 512)
        return 1;

    memset(&_ifd[ifd], 0, sizeof (_ifd[ifd]));

    _ifd[ifd].base = off;

    while (entries--)
    {
        tiffGet(base, &tag, &type, &len, &save);
        //printf("IFD-%d Tag: %d (%X)\n", ifd, tag, tag);
        switch (tag)
        {
        case 2: case 256: case 61441:	/* ImageWidth */
	        _ifd[ifd].widthTagPos = ftell(_ifp);
            _ifd[ifd].width = getInt(type);
            break;
        case 3: case 257: case 61442:	/* ImageHeight */
			_ifd[ifd].heightTagPos = ftell(_ifp);        
            _ifd[ifd].height = getInt(type);
            break;
        case 4:
            _cropTop = get2();
            //printf("_cropTop: %d\n", _cropTop);
            break;
        case 5:
            _cropLeft = get2();
            //printf("_cropLeft: %d\n", _cropLeft);
            break;
        case 6:
            _cropBottom = get2();
            //printf("_cropBottom: %d\n", _cropBottom);
            break;
        case 7:
            _cropRight = get2();
            //printf("_cropRight: %d\n", _cropRight);
            break;
        case 9:
        	_cfaPattern = get2();
        	break;
        case 0xe:	// Sat R
        	_rw2blacksatwb[3] = get2();
        	break;
        case 0xf:	// Sat G
        	_rw2blacksatwb[4] = get2();
        	break;
        case 0x10:	// Sat B
        	_rw2blacksatwb[5] = get2();
        	break;
        case 0x17:	// Pana ISO
        	_isoSpeed = get2();
        	break;
        case 0x1c:	// Black R
        	_rw2blacksatwb[0] = 15 + get2();
        	break;
        case 0x1d:	// Black G
        	_rw2blacksatwb[1] = 15 + get2();
        	break;
        case 0x1e:	// Black B
        	_rw2blacksatwb[2] = 15 + get2();
        	break;
        case 0x24:	// WB R
	        _rw2blacksatwb[6] = get2();
        	break;
        case 0x25:	// WB G
	        _rw2blacksatwb[7] = get2();
        	break;
        case 0x26:	// WB B
	        _rw2blacksatwb[8] = get2();
        	break;
        case 0x2e:	// Embedded JPEG
	        _embedJpegOffset = ftell(_ifp);
	        _embedJpegSize = len;
        	break;
        case 258:				/* BitsPerSample */
        case 61443:
            _ifd[ifd].samples = len & 7;
            _ifd[ifd].bps = getInt(type);
            break;
        case 259:				/* Compression */
            _ifd[ifd].comp = getInt(type);
            break;
        case 262:				/* PhotometricInterpretation */
            _ifd[ifd].phint = get2();
            break;
        case 270:				/* ImageDescription */
        	_descTagPos = ftell(_ifp);
            if (fread(_desc, 512, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
            break;
        case 271:				/* Make */
            if (!fgets(_make, len, _ifp)) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
	        _exif.addTag(tag)->set(EXIFTAG_ASCII, len, _make);
            break;
        case 272:				/* Model */
			_modelTagPos = ftell(_ifp);
		    if (!fgets(_model, len, _ifp)) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
	        _exif.addTag(tag)->set(EXIFTAG_ASCII, len, _model);
            break;
        case 280:               /* Panasonic RW2 offset */
            if (type != 4) break;
            _ifd[ifd].offset = get4() + base;
            _rawFormat = CR2RF_PanasonicRW2;
            break;
        case 273:				/* StripOffset */
        case 513:				/* JpegIFOffset */
        case 61447:
            _ifd[ifd].offsetTagPos = ftell(_ifp);
            _ifd[ifd].offset = get4() + base;
            if (!_ifd[ifd].bps && _ifd[ifd].offset > 0)
            {
                fseek(_ifp, _ifd[ifd].offset, SEEK_SET);
                try
                {
                    LJpegDecoder decoder(_ifp);
			        _rawHeader = decoder.getInfo();
                
                    _ifd[ifd].comp    = 6;
                    _ifd[ifd].width   = _rawHeader.wide * _rawHeader.clrs;
                    _ifd[ifd].height  = _rawHeader.high;
                    _ifd[ifd].bps     = _rawHeader.bits;
                    _ifd[ifd].samples = _rawHeader.clrs;
                    i = _order;
                    parseTiff(_ifd[ifd].offset + 12);
                    _order = i;
                }
                catch (CR2Exception e)
                {
                    if (e.getCode() != CR2E_InputFileFormatError)
                        throw e;
                }
            }
            break;
        case 274:				/* Orientation */
        	{
		        _orientationTagPos = ftell(_ifp);
		        _orientation = get2();
		        _ifd[ifd].flip = "50132467"[_orientation & 7]-'0';
	        	_exif.addTag(tag)->setUInt16(_orientation);
            }
	        break;
        case 277:				/* SamplesPerPixel */
            _ifd[ifd].samples = getInt(type) & 7;
            break;
        case 279:				/* StripByteCounts */
        case 514:
        case 61448:
            _ifd[ifd].bytesTagPos = ftell(_ifp);
            _ifd[ifd].bytes = get4();
            //printf("IFD #%d payload size: %d\n", ifd, _ifd[ifd].bytes);
            break;
        case 282:               /* X Resolution */
            _resXTagPos = ftell(_ifp);
            _resX[0] = get4();
            _resX[1] = get4();
            break;
        case 283:               /* X Resolution */
            _resYTagPos = ftell(_ifp);
            _resY[0] = get4();
            _resY[1] = get4();
            break;
        case 296:               /* Resolution Unit */
            _resUnitTagPos = ftell(_ifp);
            _resUnit = get2();
            break;
        case 306:				/* DateTime */
            getTimestamp(0);
            break;
        case 315:				/* Artist */
        	_artistTagPos = ftell(_ifp);
            if (fread(_artist, 64, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
            break;
        case 322:				/* TileWidth */
            _ifd[ifd].tile_width = getInt(type);
            break;
        case 323:				/* TileLength */
            _ifd[ifd].tile_length = getInt(type);
            break;
        case 34665:				/* EXIF tag */
            fseek(_ifp, get4()+base, SEEK_SET);
            _exifIfdPos = ftell(_ifp);
            parseExif (base);
            break;
        case 34853:				/* GPSInfo tag */
            fseek(_ifp, get4()+base, SEEK_SET);
            parseGps(base);
            break;
        case 37393:				/* ImageNumber */
        	_imageNumberTagPos = ftell(_ifp);
            _imageNumber = getInt(type);
            break;
        case 50752:         	/* CR2 Slice data */
        	_cr2SliceTagPos = ftell(_ifp);
            readShorts(_cr2Slice, 3);
            break;
        default:
            break;
        }
        fseek(_ifp, save, SEEK_SET);
    }
    
    _ifd[ifd].nextIfdTagPos = ftell(_ifp);
    _ifd[ifd].nextIfd = get4();
    fseek(_ifp, -4, SEEK_CUR);
    
    return 0;
}

int32_t CR2Raw::parseTiff(size_t base)
{
    int32_t doff;

    fseek(_ifp, base, SEEK_SET);
    _order = get2();

    if (_order != 0x4949 && _order != 0x4d4d) return 0;    
    get2();

    while ((doff = get4()))
    {
        fseek(_ifp, doff + base, SEEK_SET);
        if (parseTiffIfd(base))
			break;
        _endOfIfd = ftell(_ifp);
    }

    return 1;
}

void CR2Raw::getTimestamp(bool reversed)
{
    struct tm t;
    char str[20];
    int i;
    
    if (_timestampCount < (int32_t) COUNT(_timestampTagPos))
    {
    	_timestampTagPos[_timestampCount ++] = ftell(_ifp);
    }
    
    str[19] = 0;
    if (reversed)
        for (i = 19; i--; ) str[i] = fgetc(_ifp);
    else
        if (fread(str, 19, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
	
	memcpy(_szTimeStamp, str, 20);
	
    memset(&t, 0, sizeof (t));
    if (sscanf(str, "%d:%d:%d %d:%d:%d", &t.tm_year, &t.tm_mon,
            &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
        return;

    t.tm_year -= 1900;
    t.tm_mon -= 1;
    t.tm_isdst = -1;

    if (mktime(&t) > 0)
        _timestamp = mktime(&t);
}

void CR2Raw::parseExif (size_t base)
{
    unsigned entries, tag, type, len, save;

    entries = get2();

    while (entries--)
    {
        tiffGet(base, &tag, &type, &len, &save);
        //printf("EXIF Tag: %d (%X)\n", tag, tag);
        switch (tag) {
        case 37377:
		    _shutterTagPos2 = ftell(_ifp);
		    _shutter2[0] = get4();
		    _shutter2[1] = get4();		    	
	        _exif.addTag(tag)->setRational((int32_t*) _shutter2);
        	break;
        case 37378:
		    _apertureTagPos2 = ftell(_ifp);
		    _aperture2[0] = get4();
		    _aperture2[1] = get4();
	        _exif.addTag(tag)->setURational(_aperture2);
        	break;
        case 37380:
        	{
        		_exposureCompTagPos = ftell(_ifp);
        		_exposureComp[0] = get4();
        		_exposureComp[1] = get4();
	        	_exif.addTag(tag)->setRational(_exposureComp);
        	}
            break;
        case 33434:
		    _shutterTagPos = ftell(_ifp);
		    _shutterReal[0] = get4();
		    _shutterReal[1] = get4();
	        _exif.addTag(tag)->setRational((int32_t*) _shutterReal);
		    fseek(_ifp, -8, SEEK_CUR);
		    _shutter = (float) getReal(type);
        	break;
        case 33437:
		    _apertureTagPos = ftell(_ifp);
		    _apertureReal[0] = get4();
		    _apertureReal[1] = get4();
	        _exif.addTag(tag)->setRational((int32_t*) _apertureReal);
		    fseek(_ifp, -8, SEEK_CUR);
		    _aperture = (float) getReal(type);		    	
        	break;
        case 34850:
        	{
		    	_exposureModeTagPos = ftell(_ifp);
		    	_exposureMode = get2();
		    	int16_t mode = _exposureMode;
	        	_exif.addTag(tag)->setUInt16(mode);
	        }
        	break;
        case 34855:
        	{
	        	_isoSpeedTagPos = ftell(_ifp);
	        	uint16_t v = get2();
	        	_isoSpeed = (float) v;
	        	_exif.addTag(tag)->setUInt16(v);
	        }
        	break;
        case 36867:
        case 36868:
	        getTimestamp(false);
	        _exif.addTag(tag)->set(EXIFTAG_ASCII, 20, _szTimeStamp);
        	break;
        case 37383:
        	_meteringModeTagPos = ftell(_ifp);
        	_meteringMode = get2();
	        _exif.addTag(tag)->setUInt16(_meteringMode);
        	break;
        case 37386:
		    _focalLengthTagPos = ftell(_ifp);
		    _focalLengthReal[0] = get4();
		    _focalLengthReal[1] = get4();
	        _exif.addTag(tag)->setRational((int32_t*) _focalLengthReal); 	
		    fseek(_ifp, -8, SEEK_CUR);
		    _focalLength = (float) getReal(type);
        	break;
        case 37500:
        	{
        		char buf[12];
        		if (fread(buf, 1, 12, _ifp) < 12) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        		if (strcmp(buf, "Panasonic") == 0 || strcmp(buf, "LEICA") == 0)
        		{
                    _rawFormat = CR2RF_PanasonicRW2;
		        	parseMakerNote_Pana(base, 0);        		
        		}
        		else
        		{
                    _rawFormat = CR2RF_CanonCR2;
        			fseek(_ifp, -12, SEEK_CUR);
		        	parseMakerNote(base, 0);
        		}	
        	}
        	break;
        case 40962:		// Image width
        	_exifWidthTagPos = ftell(_ifp);
        	_exifWidth = get2();
        	break;
        case 40963:		// Image height
        	_exifHeightTagPos = ftell(_ifp);
        	_exifHeight = get2();
	        break;
        case 42033:     // Serial No (string)
		    _serialNumber2TagPos = ftell(_ifp);
		    _serialNumber2Len = len;
		    _serialNumber = get4();
		    if (fread(_serialNumber2, 1, len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
		    _serialNumber2[len] = 0;
	        _exif.addTag(tag)->set(EXIFTAG_ASCII, len, _serialNumber2); 
            break;
        case 0x9286:
        	_commentTagPos = ftell(_ifp);
        	_commentLen = len;
        	if (len > 511) len = 511;
        	if (fread(_comment, 1, len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        	_comment[len] = 0;
        	break;

        case 0x9000:
        case 0x9209:
        case 0xa001:
        case 0xa20e:
        case 0xa20f:
        case 0xa210:
        case 0xa402:
        case 0xa403:
        case 0xa406:
            {
                // Interested EXIF tags
                char buf[1024];
                if (fread(buf, TAGBYTE[type], len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
                _exif.addTag(tag)->set(type, len, buf);
            }
            break;
        default:
        	//printf("Unrecognized tag: %d(%x), type: %d, length %d\n", tag, tag, type, len);
        	break;
        }
        fseek(_ifp, save, SEEK_SET);
    }
}

void CR2Raw::parseGps(size_t base)
{
    unsigned entries, tag, type, len, save;

    entries = get2();

    while (entries--)
    {
        tiffGet(base, &tag, &type, &len, &save);
        switch (tag)
        {
        case 1: case 3: case 5:
            _gpsdata[29 + tag / 2] = getc(_ifp);
            break;
        case 2: case 4: case 7:
            FORC(6) _gpsdata[tag / 3 * 6 + c] = get4();
            break;
        case 6:
            FORC(2) _gpsdata[18 + c] = get4();
            break;
        case 18: case 29:
	       	if (!fgets((char *)(_gpsdata + 14 + tag / 3), MIN(len, 12), _ifp)) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        }
        fseek(_ifp, save, SEEK_SET);
    }
}

void CR2Raw::parseMakerNote(size_t base, int uptag)
{
    uint32_t entries, tag, type, len, save;
    int16_t morder, sorder = _order;

    entries = get2();  
    if (entries > 256) return;
    morder = _order;

    while (entries --)
    {
        _order = morder;
        tiffGet(base, &tag, &type, &len, &save);
        tag |= uptag << 16;
        
		//printf("MakerNode: tag: %d (%x), type: %d, len: %d\n", tag, tag, type, len);
        switch (tag)
        {
       	case 1:					/* Camera settings */
       		_cameraSettingsTagPos = ftell(_ifp);
       		if (len > 64) len = 64;
       		_cameraSettingsCount = len;
       		if (fread(_cameraSettings, 2, len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
       		break;       	
        case 2:					/* Focal Length */
        	_focalLengthMakerNoteTagPos = ftell(_ifp);
        	FORC4 _focalLengthMakerNote[c] = get2();
        	break;
        case 4:					/* Shot info */
       		_shotInfoTagPos = ftell(_ifp);
       		if (len > 64) len = 64;
       		_shotInfoCount = len;
       		if (fread(_shotInfo, 2, len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        	break;    
        case 9:                 /* Owner name */
            if (fread(_artist, 64, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
            break;
        case 12:                /* Camera serial number */
        	_serialNumberTagPos = ftell(_ifp);
            _serialNumber = get4();
            break;
        case 13:
            _camInfo = (uint8_t*) calloc(len, 1);
            if (fread(_camInfo, 1, len, _ifp) < len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
            break;
        case 16:
        	_modelIDTagPos = ftell(_ifp);
            _modelID = get4();
            break;
        case 149:               /* Lens model name */
        	_lensNameTagPos = ftell(_ifp);
        	_lensNameTagLen = len;
            if (fread(_lensName, len, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
            break;
        case 153:				/* Custom Functions 2 */
        	_customFunctionsTagPos = ftell(_ifp);
        	_customFunctionsCount = len;
        	_customFunctions = (int32_t*) calloc(len, 4);
        	FORC(_customFunctionsCount) _customFunctions[c] = get4();
        	break;
        case 154:               /* Aspect info */
            _aspectInfoTagPos = ftell(_ifp);
            FORC(5) _aspectInfo[c] = get4();
            break;
        case 208:               /* Canon VRD position */
            _vrdTagPos = ftell(_ifp);
            _vrdPos = get4();
            break;
        case 224:               /* Sensor dimension info, short[17] */        	
        	_sensorInfoTagPos = ftell(_ifp);
        	FORC(17) _sensorInfo[c] = get2(); 	
            break;
        case 0x4001:             /* White balance as Shot, ushort[...] */
            if (len > 500) {
            	/*int32_t pp = ftell(_ifp);            	
            	FORC(len) printf("%d ", get2());
            	printf("\n");
            	fseek(_ifp, pp, SEEK_SET);*/            
                uint32_t i = len == 582 ? 50 : len == 653 ? 48 : len == 5120 ? 142 : 126;
                int wbi = 0;
                
                fseek(_ifp, i, SEEK_CUR);
                
                // As Shot
                _wbTagPos[wbi] = ftell(_ifp);
                FORC4 _wb[c] = get2();
                _wbtemp[wbi ++] = get2();

                // Find 5200k WB and store in _wb[4..7]
                for (int i = 0; i < 20; i ++)
                {
                    uint32_t wb[4];

					size_t pos = ftell(_ifp);
                    FORC4 wb[c] = get2();
                    int temp = get2();
                    
                    switch (temp)
                    {
                    case 5200:
                        FORC4 _wb[4 + c] = wb[c];       // Daylight
    	                _wbtemp[1] = temp;
                    	_wbTagPos[1] = pos;
                        break;
                    case 7000:
                        FORC4 _wb[8 + c] = wb[c];       // Shade
    	                _wbtemp[2] = temp;	
	                    _wbTagPos[2] = pos;
                        break;
                    case 6000:
                        FORC4 _wb[12 + c] = wb[c];      // Cloudy
    	                _wbtemp[3] = temp;	
                    	_wbTagPos[3] = pos;
                        break;
                    case 3200:
                        FORC4 _wb[16 + c] = wb[c];      // Tungsten
    	                _wbtemp[4] = temp;	
                    	_wbTagPos[4] = pos;
                    	
                        //_wbTagPos[5] = ftell(_ifp);
                        FORC4 _wb[20 + c] = get2();     // Fluorescent
                        get2();
                        FORC4 get2();                   // Kelvin (ignore)
                        get2();
                        //_wbTagPos[wbi ++] = ftell(_ifp);
                        FORC4 _wb[24 + c] = get2();     // Flash
                        get2();
                        break;
                    }
                    //printf("*** WB ??? [%d]: %d %d %d %d, Temperature: %d\n", i, wb[0], wb[1], wb[2], wb[3], temp);
                }
                
                /*
                printf("*** WB Auto [%d]: %d %d %d %d\n", i, _wb[0], _wb[1], _wb[2], _wb[3]);
                printf("*** WB Daylight [%d]: %d %d %d %d\n", i, _wb[4], _wb[5], _wb[6], _wb[7]);
                printf("*** WB Shade [%d]: %d %d %d %d\n", i, _wb[8], _wb[9], _wb[10], _wb[11]);
                printf("*** WB Cloudy [%d]: %d %d %d %d\n", i, _wb[12], _wb[13], _wb[14], _wb[15]);
                printf("*** WB Tungsten [%d]: %d %d %d %d\n", i, _wb[16], _wb[17], _wb[18], _wb[19]);
                printf("*** WB Fluorescent [%d]: %d %d %d %d\n", i, _wb[20], _wb[21], _wb[22], _wb[23]);
                printf("*** WB Flash [%d]: %d %d %d %d\n", i, _wb[24], _wb[25], _wb[26], _wb[27]);*/
            }
            break;
        case 16403:             /* AFMA long[5], i.e. [20, 0, 0, 10, 0] */
            break;
        default:
        	/*printf("Unrecognized tag: %d(%x), type: %d, length %d\n", tag, tag, type, len);
            if (type == 3)
            {
            	FORC(len) printf("%d ", get2());
            	printf("\n");
            }
            else if (type == 4)
            {
            	FORC(len) printf("%d ", get4());
            	printf("\n");
            }
            else if (type == 7)
            {
            	FORC(len) printf("%c", fgetc(_ifp));
            	printf("\n");
            }
            */
        	break;
        }

        fseek(_ifp, save, SEEK_SET);
    }

    _order = sorder;
}

void CR2Raw::parseMakerNote_Pana(size_t base, int uptag)
{
    uint32_t entries, tag, type, len, save;
    int16_t morder, sorder = _order;

    entries = get2();  
    if (entries > 256) return;
    morder = _order;

    while (entries --)
    {
        _order = morder;
        tiffGet(base, &tag, &type, &len, &save);
        tag |= uptag << 16;
        
        //printf("MakerNode: tag: %d (%x), type: %d, len: %d\n", tag, tag, type, len);        
        switch (tag)
        {
        case 0x0051:
        	_lensNameTagPos = ftell(_ifp);
        	_lensNameTagLen = len;
            if (fread(_lensName, len, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        	break;
        default:
        	break;
        }
        fseek(_ifp, save, SEEK_SET);
    }

    _order = sorder;
}

void CR2Raw::parseVRD(size_t base)
{
	uint8_t buf[28];
	
	fseek(_ifp, base, SEEK_SET);	
	if (fread(buf, 28, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
	if (strncmp("CANON OPTIONAL DATA", (char*) buf, 19) != 0) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);

	_vrdLen = buf[24];
	_vrdLen = _vrdLen << 8 | buf[25];
	_vrdLen = _vrdLen << 8 | buf[26];
	_vrdLen = _vrdLen << 8 | buf[27];
	_vrdLen += 28 + 64;

	_vrd = (uint8_t*) malloc(_vrdLen);
	fseek(_ifp, base, SEEK_SET);
	if (fread(_vrd, 1, _vrdLen, _ifp) < _vrdLen) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
}

uint16_t CR2Raw::sget2(uint8_t *s)
{
    if (_order == 0x4949)
        return s[0] | s[1] << 8;
    else
        return s[0] << 8 | s[1];
}

uint16_t CR2Raw::get2()
{
    uint8_t str[2] = {0xff, 0xff};
    if (fread(str, 1, 2, _ifp) < 2) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
    return sget2(str);
}

uint32_t CR2Raw::sget4(uint8_t *s)
{
    if (_order == 0x4949)
        return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
    else
        return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}

uint32_t CR2Raw::get4()
{
    uint8_t str[4] = {0xff, 0xff, 0xff, 0xff};
    if (fread(str, 1, 4, _ifp) < 4) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
    return sget4(str);
}

uint8_t* CR2Raw::sput2(uint8_t *s, uint16_t val)
{
	if (_order == 0x4949)
	{
		s[1] = (val >> 8) & 0xff;
		s[0] = val & 0xff;
	}
	else
	{
		s[0] = (val >> 8) & 0xff;
		s[1] = val & 0xff;
	}
	
	return s;
}

void CR2Raw::put2(uint16_t val, FILE* ofp)
{
	uint8_t str[2] = {0xff, 0xff};
	if (fwrite(sput2(str, val), 1, 2, ofp) < 2) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
}

uint8_t* CR2Raw::sput4(uint8_t *s, uint32_t val)
{
	if (_order == 0x4949)
	{
		s[3] = (val >> 24) & 0xff;
		s[2] = (val >> 16) & 0xff;
		s[1] = (val >> 8) & 0xff;
		s[0] = val & 0xff;
	}
	else
	{
		s[0] = (val >> 24) & 0xff;
		s[1] = (val >> 16) & 0xff;
		s[2] = (val >> 8) & 0xff;
		s[3] = val & 0xff;
	}
	
	return s;
}

void CR2Raw::put4(uint32_t val, FILE* ofp)
{
	uint8_t str[4] = {0xff, 0xff, 0xff, 0xff};
	if (fwrite(sput4(str, val), 1, 4, ofp) < 4) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
}

uint32_t CR2Raw::getInt(int32_t type)
{
    return type == 3 ? get2() : get4();
}

float CR2Raw::intToFloat(int32_t i)
{
    union { int i; float f; } u;
    u.i = i;
    return u.f;
}

double CR2Raw::getReal(int32_t type)
{
    union { char c[8]; double d; } u;
    int i, rev;

    switch (type) {
    case 3: return(uint16_t) get2();
    case 4: return(uint32_t) get4();
    case 5:  u.d = (uint32_t) get4();
        return u.d /(uint32_t) get4();
    case 8: return(int16_t) get2();
    case 9: return(int32_t) get4();
    case 10: u.d = (int32_t) get4();
        return u.d /(int32_t) get4();
    case 11: return intToFloat(get4());
    case 12:
        rev = 7 * ((_order == 0x4949) == (ntohs(0x1234) == 0x1234));
        for (i=0; i < 8; i++)
	        u.c[i ^ rev] = fgetc(_ifp);
        return u.d;
    default: return fgetc(_ifp);
    }
}

void CR2Raw::readShorts(uint16_t *pixel, int32_t count)
{
    if (fread(pixel, 2, count, _ifp) < (uint32_t) count) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
    if ((_order == 0x4949) == (ntohs(0x1234) == 0x1234))
        _swab((char*) pixel,(char*) pixel, count*2);
}

void CR2Raw::copyBytes(FILE* ofp, FILE* ifp, size_t bytes)
{
    const int32_t bufsize = 16384;
	uint8_t buf[bufsize];
    int32_t count = bytes;

	while (count > 0)
	{
		size_t w = (count > bufsize) ? bufsize : count;
		if (fread(buf, 1, w, ifp) < w) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
		if (fwrite(buf, 1, w, ofp) < w) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		count -= w;
	}
}

void CR2Raw::writeBytes(FILE* ofp, void* buf, size_t bytes)
{
    const int32_t bufsize = 16384;
    int32_t count = bytes;
    uint8_t* bbuf = (uint8_t*) buf;

	while (count > 0)
	{
		size_t w = (count > bufsize) ? bufsize : count;
        if (fwrite(bbuf, 1, w, ofp) < w) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		count -= w;
        bbuf += w;
	}
}

uint8_t* CR2Raw::getSensorRGBPreview(int32_t w, int32_t h, int32_t bits)
{
    uint8_t* buf = (uint8_t*) malloc(w * h * 3 * bits / 8);

    // Scan sensor data and generate RGB output
    const int32_t xstart = _sensorInfo[5] + 8, xsize = _sensorInfo[7] - _sensorInfo[5] - 16;
    const int32_t ystart = _sensorInfo[6] + 8, ysize = _sensorInfo[8] - _sensorInfo[6] - 16;
    const float xstep = (float) xsize / (float) (w + 1);
    const float ystep = (float) ysize / (float) (h + 1);
    const int32_t rw = _rawWidth;
	uint8_t tcurve[3590];
	static const int32_t _curve[] = 
		{0,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,
		102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,
		178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,
		254,258,262,266,270,274,278,282,286,290,294,298,304,308,312,316,320,324,328,
		332,336,340,346,350,354,360,364,368,374,378,384,388,394,400,404,410,416,420,
		426,432,438,444,452,458,464,472,478,486,494,500,510,518,526,534,542,552,560,
		568,578,586,596,604,614,624,632,642,652,662,672,682,692,704,714,726,736,748,
		758,770,782,794,806,820,834,846,860,874,888,904,920,936,952,970,988,1004,1022,
		1040,1058,1076,1094,1112,1130,1148,1166,1184,1202,1222,1240,1258,1276,1296,1314,
		1334,1352,1372,1390,1410,1428,1448,1468,1488,1506,1526,1546,1566,1586,1606,1626,
		1646,1668,1688,1708,1730,1752,1772,1794,1816,1840,1862,1886,1908,1932,1956,1982,
		2006,2032,2058,2084,2110,2138,2166,2194,2222,2250,2280,2310,2340,2370,2400,2430,
		2462,2492,2524,2554,2586,2618,2650,2682,2714,2746,2780,2812,2846,2880,2914,2948,
		2982,3018,3052,3088,3124,3160,3196,3232,3270,3306,3344,3382,3420,3460,3498,3538,
		3578,3585};	
	
    if (bits == 8)
    {
		for (int32_t i = 0; i < 3590; i ++)
		{
			tcurve[i] = 255;
			for (int32_t j = 0; j < 256; j ++)
			{
				if (i <= _curve[j])
				{
					tcurve[i] = j;
					break;
				}
			}
		}    
    }
    
    for (int32_t i = 0; i < h; i ++)
    {
    	int32_t row = (int32_t) (i * ystep + ystart);
    	
        if (bits == 16) {
        	const uint16_t* src = _rawImage + ((row >> 1) << 1) * rw;
            uint16_t* dst = (uint16_t*) buf + i * w * 3;

            for (int32_t j = 0; j < w; j ++)
            {
                const uint16_t* p = src + ((((int32_t) (j * xstep + xstart)) >> 1) << 1);

                *dst ++ = p[0];
                *dst ++ = p[1];
                *dst ++ = p[rw + 1];
            }
        }
        else
        {
        	const uint16_t* src = _rawImage + (((row >> 1) << 1) + 1) * rw;
            uint8_t* dst = buf + i * w * 3;

            for (int32_t j = 0; j < w; j ++)
            {
                const uint16_t* p = src + ((((int32_t) (j * xstep + xstart)) >> 1) << 1);
				int32_t r = (p[0] - 127), g = (p[1] - 127), b = (p[rw + 1] - 127);
				if (r < 0) r = 0; else if (r > 3590) r = 3590;
				if (g < 0) g = 0; else if (g > 3590) g = 3590;
				if (b < 0) b = 0; else if (b > 3590) b = 3590;
                *dst ++ = tcurve[r];
                *dst ++ = tcurve[g];
                *dst ++ = tcurve[b];
            }
        }
    }

    return buf;
}

// RW2Decoder -------------------------------------------------------------------------------
// Decode Panasonic RW2 raw section

RW2Decoder::RW2Decoder(FILE* fp)
{
    memset(this, 0, sizeof (*this));
    _ifp = fp;
    _load_flags = 0x2008;
}

uint32_t RW2Decoder::pana_bits(int nbits)
{
    int32_t byte;

    if (!nbits) return _vbits=0;
    if (!_vbits)
    {
        if (fread (_buf + _load_flags, 1, 0x4000 - _load_flags, _ifp) < (uint32_t) (0x4000 - _load_flags)) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        if (fread (_buf, 1, _load_flags, _ifp) < (uint32_t) _load_flags) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
    }
    _vbits = (_vbits - nbits) & 0x1ffff;
    byte = _vbits >> 3 ^ 0x3ff0;

    return (_buf[byte] | _buf[byte + 1] << 8) >> (_vbits & 7) & ~(-1 << nbits);
}

void RW2Decoder::getStream(uint16_t* data, const int32_t w, const int32_t h, const int32_t cropRight)
{
    int32_t i, j, sh = 0, pred[2], nonz[2];

    pana_bits(0);

    for (int32_t row = 0; row < h; row ++)
    {
        for (int32_t col = 0; col < w; col ++) 
        {
            if ((i = col % 14) == 0)
                pred[0] = pred[1] = nonz[0] = nonz[1] = 0;

            if (i % 3 == 2)
                sh = 4 >> (3 - pana_bits(2));

            if (nonz[i & 1])
            {
                if ((j = pana_bits(8)))
                {
	                if ((pred[i & 1] -= 0x80 << sh) < 0 || sh == 4)
	                    pred[i & 1] &= ~(-1 << sh);
	                pred[i & 1] += j << sh;
                }
            }
            else if ((nonz[i & 1] = pana_bits(8)) || i > 11)
            {
                pred[i & 1] = nonz[i & 1] << 4 | pana_bits(4);
            }

            if ((data[row * w + col] = pred[col & 1]) > 4098 && col <= cropRight) throw -1;
        }
    }
}

// LJpegDecoder -------------------------------------------------------------------------------
// Decode Canon CR2, Kodak, Adobe DNG raw section

LJpegDecoder::LJpegDecoder(FILE* fp)
{
	memset(this, 0, sizeof (LJpegDecoder));	
	_ifp = fp;
}

uint32_t LJpegDecoder::getBitHuff(int32_t nbits, uint16_t *huff)
{
    uint32_t c;

    if (nbits > 25) return 0;
    if (nbits < 0) return _bitbuf = _vbits = _reset = 0;
    if (nbits == 0 || _vbits < 0) return 0;

    while (!_reset && _vbits < nbits && (c = fgetc(_ifp)) != EOF)
    {
        _bitbuf = (_bitbuf << 8) + (uint8_t) c;
        _vbits += 8;
                
        if (c == 0xff)
        	fgetc(_ifp);
    }

    c = _bitbuf << (32 - _vbits) >> (32 - nbits);
    if (huff)
    {
        _vbits -= huff[c] >> 8;
        c = (uint8_t) huff[c];
    }
    else
        _vbits -= nbits;

    if (_vbits < 0) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);

    return c;
}

uint16_t* LJpegDecoder::makeDecoderRef(const uint8_t **source)
{
    int max, len, h, i, j;
    const uint8_t *count;
    uint16_t *huff;

    count = (*source += 16) - 17;
    for (max = 16; max && !count[max]; max --);
        
    huff = (uint16_t*) calloc(1 + (1 << max), sizeof (*huff));    
    if (!huff) throw CR2Exception(__FILE__, __LINE__, CR2E_OutOfMemory);

    huff[0] = max;
    for (h = len = 1; len <= max; len ++)
        for (i = 0; i < count[len]; i ++, ++ *source)
            for (j = 0; j < 1 << (max - len); j++)
                if (h <= 1 << max)
	                huff[h ++] = len << 8 | **source;
	
    return huff;
}

void LJpegDecoder::ljpegStart(bool info_only)
{
    int32_t c, tag, len;
    uint8_t data[4096];

    if (fread(data, 2, 1, _ifp) < 1) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
    if (data[1] != 0xd8) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);

    do
    {
        if (fread(data, 2, 2, _ifp) < 2) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);
        tag = data[0] << 8 | data[1];
        len = (data[2] << 8 | data[3]) - 2;

        if (tag <= 0xff00) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);

        if (fread(data, 1, len, _ifp) < (size_t) len) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileReadError);

        switch (tag)
        {
        case 0xffc3:        // SOF3
        case 0xffc0:        // SOF0 - Start Of Frame 0
            _jh.bits = data[0];
            _jh.high = data[1] << 8 | data[2];
            _jh.wide = data[3] << 8 | data[4];
            _jh.clrs = data[5];
            /*printf("SOF (%d bytes)\n", len);
            for (int i = 0; i < len; i ++) printf("%02X ", data[i]);
            printf("\n");*/
            break;
        case 0xffc4:        // DHT - Define Huffman Table
            if (info_only) break;
            for (const uint8_t* dp = data; dp < data + len && (c = *dp++) < 2; )
                _huff[c] = makeDecoderRef(&dp);
            break;
        case 0xffda:        // SOS - Start Of Scan
            _jh.psv = data[1 + data[0] * 2];
            _jh.bits -= data[3 + data[0] * 2] & 15;
            break;
        case 0xffdb:        // DQT - Define Quantization Table(Thumbnail only)
            break;
        default:
            //printf("Unrecognized Marker: %d(%x), length %d\n", tag, tag, len);
            break;
        }
    } while (tag != 0xffda);

    if (info_only) return;
    if (_jh.clrs > 4 || !_huff[0]) throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);	
	for (int i = 1; i < 4; i ++) if (!_huff[i]) _huff[i] = _huff[i - 1];

    _row = (uint16_t*) calloc(_jh.wide * _jh.clrs, sizeof (uint16_t));
    if (!_row) throw CR2Exception(__FILE__, __LINE__, CR2E_OutOfMemory);
}

void LJpegDecoder::ljpegEnd()
{
    for (int i = 3; i > 0; i --) if (_huff[i] != _huff[i - 1]) free(_huff[i]);
    free(_huff[0]);
    free(_row);
}

int32_t LJpegDecoder::ljpegDiff(uint16_t* huff)
{
    const int32_t len = getBitHuff(*huff, huff + 1);
    int32_t diff = getBitHuff(len, 0);

    if ((diff & (1 << (len-1))) == 0)
        diff -= (1 << len) - 1;

    return diff;
}

uint16_t* LJpegDecoder::ljpegRow(int32_t jrow)
{
    // Start of scan
    if (jrow == 0)
    {
        FORC(4) _vpred[c] = 1 << (_jh.bits - 1);
        getBitHuff(-1, 0);
    }

    // Decompress a line of pixels
    uint16_t* row = _row;
    for (int32_t col = 0; col < _jh.wide; col ++)
    FORC(_jh.clrs)
    {
        const int32_t diff = ljpegDiff(_huff[c]);
        const int32_t pred = col ? row[-_jh.clrs] : (_vpred[c] += diff) - diff;

        if ((*row = pred + diff) >> _jh.bits)
        {
            throw CR2Exception(__FILE__, __LINE__, CR2E_InputFileFormatError);
        }
        
        row ++;
    }

    return _row;
}

const JpegHeader &LJpegDecoder::getInfo()
{
	size_t pos = ftell(_ifp);	
	ljpegStart(true);
	fseek(_ifp, pos, SEEK_SET);
	
	return _jh;
}

int32_t LJpegDecoder::getStream(uint16_t* data, uint16_t* slices)
{
    ljpegStart(false);

    const int32_t jwide = _jh.wide * _jh.clrs;
    int32_t row = 0, col = 0;

    for (int32_t jrow = 0; jrow < _jh.high; jrow ++)
    {
        const uint16_t* rp = ljpegRow(jrow);
        for (int32_t jcol = 0; jcol < jwide; jcol ++)
        {
            if (slices && slices[0])
            {
                int32_t jidx = jrow * jwide + jcol;
                int32_t i = jidx / (slices[1] * _jh.high), j;

                if ((j = i >= slices[0]))
                    i = slices[0];

                jidx -= i * (slices[1] * _jh.high);
                row = jidx / slices[1 + j];
                col = jidx % slices[1 + j] + i * slices[1];
            }
            if (row < (int32_t) _jh.high) data[row * jwide + col] = *rp ++;
            if (++ col >= (int32_t) jwide) col = (row ++, 0);
        }
    }

    ljpegEnd();
    
    return 1;
}

// LJpegEncoder -------------------------------------------------------------------------------
// Encode Canon CR2 raw section
/*
14-bit Huffman Table:
00             4
01             5
100            3
101            6
1100           7
1101           2
11100          8
11101          0
11110          1
111110         9
1111110        10
11111110       12
111111110      11
1111111110     13
11111111110    14
*/
LJpegEncoder::LJpegEncoder(FILE* fp)
{
	memset(this, 0, sizeof (LJpegEncoder));	
	_ofp = fp;
}

void LJpegEncoder::setInfo(const JpegHeader& jh)
{
	_jh = jh;
}

int32_t LJpegEncoder::setStream(uint16_t* data, uint16_t* slices)
{
	ljpegStart();

    const int32_t jwide = _jh.wide * _jh.clrs;
    int32_t row = 0, col = 0;

    for (int32_t jrow = 0; jrow < _jh.high; jrow ++)
    {
        uint16_t* rp = _row;
        for (int32_t jcol = 0; jcol < jwide; jcol ++)
        {
            if (slices && slices[0])
	        {
	            int32_t jidx = jrow * jwide + jcol;
	            int32_t i = jidx / (slices[1] * _jh.high), j;

	            if ((j = i >= slices[0]))
	                i = slices[0];

	            jidx -= i * (slices[1] * _jh.high);
	            row = jidx / slices[1 + j];
	            col = jidx % slices[1 + j] + i * slices[1];
	        }

	        if (row < (int32_t) _jh.high) *rp ++ = data[row * jwide + col];
	        if (++ col >= (int32_t) jwide) col = (row ++, 0);
        }
		ljpegRow(jrow);
    }
    
	ljpegEnd();
	return 0;
}

void LJpegEncoder::putMarker(int32_t tag, int32_t len, uint8_t* data)
{
	const uint32_t mlen = len + 2;
	uint8_t d[4] = {(tag >> 8) & 0xff, tag & 0xff, (mlen >> 8) & 0xff, mlen & 0xff};
	
	if (fwrite(d, len > 0 ? 2 : 1, 2, _ofp) < 2) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	
	if (len > 0)
	{
		if (fwrite(data, 1, len, _ofp) < (size_t) len) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
	}
}

int32_t LJpegEncoder::putBit(int32_t nbits, uint32_t val)
{
	if (nbits > 25) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileFormatError);
	if (nbits < 0) return _bitbuf = _vbits = _reset = 0;
	
	while (_vbits >= 8)
	{
		uint8_t c = (_bitbuf >> 24) & 0xff;
				
		if (fputc(c, _ofp) < 0) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		if (c == 0xff)
			if (fputc(0, _ofp) < 0) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		
		_bitbuf <<= 8;
		_vbits -= 8;
	}
	
	// Flush buffer, end of scan
	if (nbits == 0 && _vbits > 0)
	{
		uint8_t c = (_bitbuf >> 24) & 0xff;
		if (fputc(c, _ofp) < 0) throw CR2Exception(__FILE__, __LINE__, CR2E_OutputFileWriteError);
		return 0;
	}

	_bitbuf |= val << (32 - nbits) >> _vbits;
	_vbits += nbits;

	return nbits;
}

void LJpegEncoder::ljpegStart()
{
	// 12/14-bit data stream                 // 1  2  3  4  5  6  7  8  9  10 11 .  .  .  .  .
	static const uint8_t HUFF_TABLE_DEF_14[] = {0, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
	static const uint8_t HUFF_TABLE_DEF_12[] = {0, 2, 2, 2, 3, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
	static const uint8_t HUFF_TABLE_VAL[] = {4, 5, 3, 6, 7, 2, 8, 0, 1, 9, 10, 12, 11, 13, 14};
														                           //  ^ 12-bit end
	uint8_t data[1024];
	
	putMarker(0xffd8, 0, 0);
	
	// DHT
	const int32_t htlen = 1 + 16 + _jh.bits + 1;
	FORC(2)
	{
		data[htlen * c] = c;
		memcpy(data + htlen * c + 1, _jh.bits == 14 ? HUFF_TABLE_DEF_14 : HUFF_TABLE_DEF_12, 16);
		memcpy(data + htlen * c + 17, HUFF_TABLE_VAL, _jh.bits + 1);		
	}	
	putMarker(0xffc4, htlen * 2, data);
	
	// SOF
	data[0] = _jh.bits;
	data[1] = (_jh.high >> 8) & 0xff;
	data[2] = _jh.high & 0xff;
	data[3] = (_jh.wide >> 8) & 0xff;
	data[4] = _jh.wide & 0xff;
	data[5] = _jh.clrs;
	data[6] = 0x01;		data[7] = 0x11;		data[8] = 0x00;
	data[9] = 0x02;		data[10] = 0x11;	data[11] = 0x00;
	putMarker(0xffc3, 12, data);

	// SOS
	data[0] = 2;	data[1] = 1;	data[2] = 0;	data[3] = 2;
	data[4] = 16;	data[5] = 1;	data[6] = 0;	data[7] = 0;
	putMarker(0xffda, 8, data);

	_row = (uint16_t*) calloc(_jh.wide * _jh.clrs, sizeof (uint16_t));
    if (!_row) throw CR2Exception(__FILE__, __LINE__, CR2E_OutOfMemory);
}

void LJpegEncoder::ljpegEnd()
{
	putMarker(0xffd9, 0, 0);
	free(_row);
}

#ifdef WIN32
uint32_t __inline __builtin_clz( uint32_t value )
{
    DWORD leading_zero = 0;
    if (_BitScanReverse(&leading_zero, value))
    {
		return 31 - leading_zero;
    }
    else
    {
		return 32;
    }
}
#endif

void LJpegEncoder::ljpegDiff(int32_t diff)
{
		                                     // 0   1   2   3  4  5  6  7   8   9   10   11   12   13    14
	static const uint8_t  HUFF_REVERSE_LEN[] = {5,  5,  4,  3, 2, 2, 3, 4,  5,  6,  7,   9,   8,   10,   11};
	static const uint16_t HUFF_REVERSE_VAL[] = {29, 30, 13, 4, 0, 1, 5, 12, 28, 62, 126, 510, 254, 1022, 2046};
	int32_t len;
	
	if (diff > 0)
	{
		len = 32 - __builtin_clz(diff);
	}
	else if (diff < 0)
	{
		int32_t adiff = -diff;
		len = 32 - __builtin_clz(adiff);
		diff = ~adiff;
	}
	else
	{
		diff = 0;
		len = 0;
	}
	
	assert(len <= 14);
	putBit(HUFF_REVERSE_LEN[len], HUFF_REVERSE_VAL[len]);
	if (len > 0)
		putBit(len, diff);
}

void LJpegEncoder::ljpegRow(int32_t jrow)
{
	// Start of scan
	if (jrow == 0)
	{
		FORC(2) _vpred[c] = 1 << (_jh.bits - 1);
        putBit(-1, 0);
	}
	
	// Compress a line of pixels
    uint16_t* row = _row;
    for (int32_t col = 0; col < _jh.wide; col ++)
    FORC(_jh.clrs)
    {
        const int32_t pred = col ? row[-_jh.clrs] : _vpred[c];
        const int32_t diff = *row - pred;
        if (!col)
        	_vpred[c] += diff;
        
        ljpegDiff(diff);
        
        row ++;
    }
    
    // End of scan
    if (jrow == _jh.high - 1)
    	putBit(0, 0);
}
