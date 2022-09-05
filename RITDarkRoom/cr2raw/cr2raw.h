#include <stdio.h>
#include <stdint.h>
#include "exif.h"
#include "os.h"

#ifndef __CR2RAW__
#define __CR2RAW__

#define FORC(cnt) for (int c=0; c < cnt; c++)
#define FORC2 FORC(2)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)
#define COUNT(p) (sizeof (p) / sizeof (p[0]))

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

struct TiffIfd {
	uint16_t width, height, bps, comp, phint;
    int32_t offset;
    uint32_t flip, samples, bytes;
	uint16_t tile_width, tile_length;

    size_t nextIfd, nextIfdTagPos;
	size_t base, widthTagPos, heightTagPos, offsetTagPos, bytesTagPos;

    void* payload;
    size_t payloadSize;
};

struct JpegHeader {
	int32_t bits, high, wide, clrs, psv;
};

class LJpegDecoder
{
public:
	LJpegDecoder(FILE* fp);

	const JpegHeader &getInfo();
    int32_t     getStream(uint16_t* data, uint16_t* slices);

private:
    // Lossless Jpeg
    uint32_t    getBitHuff(int32_t nbits, uint16_t* huff);
    uint16_t*   makeDecoderRef(const uint8_t** source);
    void     	ljpegStart(bool info_only);
    void        ljpegEnd();
    int32_t     ljpegDiff(uint16_t* huff);
    uint16_t*   ljpegRow(int32_t jrow);

private:
	JpegHeader	_jh;
	uint16_t*	_row;
	uint32_t 	_vpred[4];
	uint16_t*	_huff[4];
    uint32_t    _bitbuf;
    int32_t     _vbits, _reset;
    FILE*		_ifp;
};

class LJpegEncoder
{
public:
	LJpegEncoder(FILE* fp);
	
	void		setInfo(const JpegHeader& jh);
	int32_t		setStream(uint16_t* data, uint16_t* slices);
	
private:
	void		putMarker(int32_t tag, int32_t len, uint8_t* data);
	int32_t		putBit(int32_t nbits, uint32_t val);
	void		ljpegStart();
	void		ljpegEnd();
	void		ljpegDiff(int32_t diff);
	void		ljpegRow(int32_t jrow);
	
private:
	JpegHeader	_jh;
	uint16_t*	_row;
	uint32_t	_vpred[4];
	uint32_t	_bitbuf;
	int32_t		_vbits, _reset;
	FILE*		_ofp;
};

class RW2Decoder
{
public:
    RW2Decoder(FILE* fp);
    void        getStream(uint16_t* data, const int32_t w, const int32_t h, const int32_t cropRight);

private:    
     uint32_t   pana_bits(int nbits);

private:
    uint8_t     _buf[0x4000];
    int32_t     _vbits;
    int32_t     _load_flags;
    FILE*       _ifp;
};

enum CR2ExceptionCode
{
    CR2E_Unknown = 0,
    CR2E_OutOfMemory,
    CR2E_InputFileNotFound,
    CR2E_CannotCreateOutputFile,
    CR2E_InputFileReadError,
    CR2E_OutputFileWriteError,
    CR2E_InputFileFormatError,
    CR2E_OutputFileFormatError
};

class CR2Exception
{
public:
    CR2Exception(const char* file, int line, const CR2ExceptionCode code);

    void                    print();
    const CR2ExceptionCode  getCode();
    const char*             getMessage();

private:
    const char*             _file;
    int                     _line;
    CR2ExceptionCode        _code;
};

enum CR2RawFormat
{
    CR2RF_CanonCR2,
    CR2RF_PanasonicRW2
};

class CR2Raw
{
public:
    CR2Raw(const char* fname);
    ~CR2Raw();

    uint16_t*			createRawData(int32_t w, int32_t h, uint16_t black);
    uint16_t*			getRawData();

    const int32_t		getWidth() const;
    const int32_t		getHeight() const;
    const uint32_t		getCfaType() const;

    uint32_t            getModelID() const;
    uint16_t            getLensID() const;
    const char*			getModel() const;
    const char*         getSerialNumber() const;
    const char*         getLensModel() const;
    int32_t             getFocalLength() const;
    size_t				getCommentLen() const;
    char*				getComment();

    const uint16_t      getBps() const;
	const int32_t		getISO() const;
	uint32_t*     		getWBCoefficients();
	uint32_t*			getWBTemperature();
	uint16_t*			getSliceData();
	uint16_t*			getSensorInfo();
	uint32_t*			getRW2BlackSatWB();	
	CR2RawFormat		getRawFormat() const;

	void				copyExif(const CR2Raw* src);
	void				setModelID(uint32_t modelID);
	void				save(const char* fname, CR2Raw* thumbs = NULL, StreamReader* ljpeg = NULL);
    void                loadVRD(const char* fname);
	bool				saveVRD(const char* fname, size_t* pos = NULL, size_t* len = NULL);
    void                saveRawData(const char* fname);
	void                reformat(const char* fname);
    ExifIfd       		&getExif();

private:
    // Tiff
    void        tiffGet(size_t base, uint32_t* tag, uint32_t* type, uint32_t* len, uint32_t* save);
    int32_t     parseTiffIfd(size_t base);
    int32_t     parseTiff(size_t base);

    // Exif
    void        getTimestamp(bool reversed);
    void        parseExif(size_t base);
    void        parseGps(size_t base);
    void        parseMakerNote(size_t base, int uptag);
    void        parseMakerNote_Pana(size_t base, int uptag);
	void		parseVRD(size_t base);

    // Helpers
    uint16_t    sget2(uint8_t *s);
    uint16_t    get2();
    uint32_t    sget4(uint8_t *s);
    uint32_t    get4();
    uint32_t    getInt(int32_t type);
    float       intToFloat(int32_t i);
    double      getReal(int32_t type);
    void        readShorts(uint16_t *pixel, int32_t count);
	uint8_t*	sput2(uint8_t *s, uint16_t val);
	void 		put2(uint16_t val, FILE* ofp);
	uint8_t*	sput4(uint8_t *s, uint32_t val);
	void 		put4(uint32_t val, FILE* ofp);	
    void        copyBytes(FILE* ofp, FILE* ifp, size_t bytes);
    void        writeBytes(FILE* ofp, void* buf, size_t bytes);
    uint8_t*    getSensorRGBPreview(int32_t w, int32_t h, int32_t bits);

private:
	char*		_fileName;

    // Exif data
    uint32_t    _modelID, _lensID;
    char        _desc[512], _lensName[512];
    char        _make[64], _model[64], _artist[64];
    char		_comment[512];
    float       _isoSpeed, _shutter, _aperture, _focalLength;
    char		_szTimeStamp[20], _serialNumber2[64];
    time_t      _timestamp;
    uint32_t    _serialNumber;
    uint32_t    _imageNumber;
    uint32_t    _gpsdata[32];
    uint32_t    _wb[32], _rw2blacksatwb[12], _wbtemp[32];
	
    uint32_t    _aspectInfo[5];
	uint32_t	_shutterReal[2], _apertureReal[2], _focalLengthReal[2];
    uint32_t    _shutter2[2], _aperture2[2];
	int32_t		_exposureComp[2];
    uint32_t    _resX[2], _resY[2];
    uint16_t	_meteringMode;
    uint16_t    _resUnit, _orientation, _exposureMode;
	uint16_t	_focalLengthMakerNote[4];
	uint16_t	_cameraSettings[64];
	uint16_t	_shotInfo[64];
	uint16_t	_sensorInfo[17];
    uint8_t*    _camInfo;
	int32_t*	_customFunctions;
	int32_t		_timestampCount;
	size_t		_commentLen;
	
	size_t		_modelIDTagPos;
	size_t		_modelTagPos;
    size_t      _aspectInfoTagPos, _exposureModeTagPos;
	size_t		_descTagPos, _lensNameTagPos, _lensNameTagLen, _artistTagPos;
	size_t		_isoSpeedTagPos, _shutterTagPos, _apertureTagPos, _focalLengthTagPos;
    size_t      _shutterTagPos2, _apertureTagPos2;
	size_t		_exposureCompTagPos;
    size_t		_meteringModeTagPos;
    size_t      _resXTagPos, _resYTagPos, _resUnitTagPos, _orientationTagPos;
	size_t		_focalLengthMakerNoteTagPos;
	size_t      _timestampTagPos[4];
    size_t    	_serialNumberTagPos, _serialNumber2TagPos, _serialNumber2Len;
    size_t    	_imageNumberTagPos;
	size_t		_wbTagPos[8];
	size_t		_cameraSettingsTagPos, _shotInfoTagPos;
	int32_t		_cameraSettingsCount, _shotInfoCount;
	size_t		_customFunctionsTagPos;
	int32_t		_customFunctionsCount;
	size_t		_commentTagPos;
    size_t      _exifIfdPos;
	
	size_t		_embedJpegOffset, _embedJpegSize;
	
    uint32_t    _vrdTagPos, _vrdPos, _vrdLen;
    uint32_t	_sensorInfoTagPos, _cr2SliceTagPos;
    
    // Raw data
    CR2RawFormat _rawFormat;
    uint32_t	_rawSizeTagPos;
    uint16_t*   _rawImage;
    uint16_t    _rawWidth, _rawHeight;
    uint16_t    _exifWidth, _exifHeight;
    uint32_t	_exifWidthTagPos, _exifHeightTagPos;
    uint32_t    _rawBps;
    size_t      _rawOffset;
    uint16_t    _rawFlip, _rawCompress;
    uint16_t    _cr2Slice[3];
    uint32_t    _rawIfd;
    uint8_t*	_vrd;
    JpegHeader	_rawHeader;

    // Panasonic specific
    uint16_t    _cropLeft, _cropTop, _cropRight, _cropBottom;
    uint32_t	_cfaPattern;

    // Misc
    uint16_t    _order;
    TiffIfd     _ifd[8];
    uint32_t    _ifdCount;
    FILE*       _ifp;
    size_t      _endOfIfd;
    ExifIfd		_exif;
};

#endif
