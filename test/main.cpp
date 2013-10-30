// 
// (C) Jan de Vaan 2007-2010, all rights reserved. See the accompanying "License.txt" for licensed use. 
// 


#include "config.h"
#include <sstream>
#include <fstream>

#include <vector>

#include "../interface.h"
#include "../util.h"
#include "../header.h"
#include "../defaulttraits.h"
#include "../losslesstraits.h"
#include "../colortransform.h"
#include "../jpegmarker.h"
#include "../processline.h"

#include "util.h"

typedef const char* SZC;

const std::ios_base::openmode mode_input  = std::ios_base::in  | std::ios::binary;
const std::ios_base::openmode mode_output = std::ios_base::out | std::ios::binary;

bool ScanFile(SZC strNameEncoded, std::vector<BYTE>* rgbyteFile, JlsParameters* info)
{
	if (!ReadFile(strNameEncoded, rgbyteFile))
	{
		ASSERT(false);
		return false;
	}
	std::basic_filebuf<char> jlsFile; 
	jlsFile.open(strNameEncoded, mode_input);

	ByteStreamInfo rawStreamInfo = {&jlsFile};

	JLS_ERROR err = JpegLsReadHeaderStream(rawStreamInfo, info);
	ASSERT(err == OK);
	return err == OK;
}




void TestTraits16bit()
{
	DefaultTraitsT<USHORT,USHORT> traits1 = DefaultTraitsT<USHORT,USHORT>(4095,0);
	LosslessTraitsT<USHORT,12> traits2 = LosslessTraitsT<USHORT,12>();

	ASSERT(traits1.LIMIT == traits2.LIMIT);
	ASSERT(traits1.MAXVAL == traits2.MAXVAL);
	ASSERT(traits1.RESET == traits2.RESET);
	ASSERT(traits1.bpp == traits2.bpp);
	ASSERT(traits1.qbpp == traits2.qbpp);

	for (int i = -4096; i < 4096; ++i)
	{
		ASSERT(traits1.ModRange(i) == traits2.ModRange(i));
		ASSERT(traits1.ComputeErrVal(i) == traits2.ComputeErrVal(i));
	}

	for (int i = -8095; i < 8095; ++i)
	{
		ASSERT(traits1.CorrectPrediction(i) == traits2.CorrectPrediction(i));
		ASSERT(traits1.IsNear(i,2) == traits2.IsNear(i,2));	
	}
}

void TestTraits8bit()
{
	DefaultTraitsT<BYTE,BYTE> traits1 = DefaultTraitsT<BYTE,BYTE>(255,0);
	LosslessTraitsT<BYTE,8> traits2 = LosslessTraitsT<BYTE,8>();

	ASSERT(traits1.LIMIT == traits2.LIMIT);
	ASSERT(traits1.MAXVAL == traits2.MAXVAL);
	ASSERT(traits1.RESET == traits2.RESET);
	ASSERT(traits1.bpp == traits2.bpp);
	ASSERT(traits1.qbpp == traits2.qbpp);	

	for (int i = -255; i < 255; ++i)
	{
		ASSERT(traits1.ModRange(i) == traits2.ModRange(i));
		ASSERT(traits1.ComputeErrVal(i) == traits2.ComputeErrVal(i));
	}

	for (int i = -255; i < 512; ++i)
	{
		ASSERT(traits1.CorrectPrediction(i) == traits2.CorrectPrediction(i));
		ASSERT(traits1.IsNear(i,2) == traits2.IsNear(i,2));	
	}
}

std::vector<BYTE> MakeSomeNoise(int length, int bitcount, int seed)
{
	srand(seed);
	std::vector<BYTE> rgbyteNoise(length);
	BYTE mask = (1 << bitcount) - 1;
	for (int icol= 0; icol<length; ++icol)
	{
		BYTE val = BYTE(rand());
		rgbyteNoise[icol] = BYTE(val & mask);
	}	
	return rgbyteNoise;
}


void TestNoiseImage()
{
	Size size2 = Size(512, 512);	
		
	for (int bitDepth = 8; bitDepth >=2; --bitDepth)
	{
		std::stringstream label;
		label << "noise, bitdepth: " << bitDepth;

		std::vector<BYTE> noiseBytes = MakeSomeNoise(size2.cx * size2.cy, bitDepth, 21344);
		TestRoundTrip(label.str().c_str(), noiseBytes, size2, bitDepth, 1);
	}
}
 

void TestFailOnTooSmallOutputBuffer()
{
	Size size = Size(8, 8);
	std::vector<BYTE> rgbyteRaw = MakeSomeNoise(8 * 8, 8, 21344);

	std::vector<BYTE> rgbyteCompressed(1);
 	 
	JlsParameters info = JlsParameters();
	info.components = 1;
	info.bitspersample = 8;
	info.height = size.cy;
	info.width = size.cx;

	size_t compressedLength;
	JLS_ERROR err = JpegLsEncode(&rgbyteCompressed[0], rgbyteCompressed.size(), &compressedLength, &rgbyteRaw[0], rgbyteRaw.size(), &info);
	ASSERT(err == CompressedBufferTooSmall);
}



void TestBgra()
{
	char rgbyteTest[] = "RGBARGBARGBARGBA1234";
	char rgbyteComp[] = "BGRABGRABGRABGRA1234";
	TransformRgbToBgr(rgbyteTest, 4, 4);
	ASSERT(strcmp(rgbyteTest, rgbyteComp) == 0);
}


void TestBgr()
{
	JlsParameters info;
	std::vector<BYTE> rgbyteEncoded;	
	ScanFile("test/conformance/T8C2E3.JLS", &rgbyteEncoded, &info);
	std::vector<BYTE> rgbyteDecoded(info.width * info.height * info.components);	

	info.outputBgr = true;

	JLS_ERROR err = JpegLsDecode(&rgbyteDecoded[0], rgbyteDecoded.size(), &rgbyteEncoded[0], rgbyteEncoded.size(), &info);
	ASSERT(err == OK);

	ASSERT(rgbyteDecoded[0] == 0x69);
	ASSERT(rgbyteDecoded[1] == 0x77);
	ASSERT(rgbyteDecoded[2] == 0xa1);
	ASSERT(rgbyteDecoded[info.width * 6 + 3] == 0x2d);
	ASSERT(rgbyteDecoded[info.width * 6 + 4] == 0x43);
	ASSERT(rgbyteDecoded[info.width * 6 + 5] == 0x4d);	

}


void TestTooSmallOutputBuffer()
{
	std::vector<BYTE> rgbyteCompressed;	
	if (!ReadFile("test/lena8b.jls", &rgbyteCompressed, 0))
		return;

	std::vector<BYTE> rgbyteOut(512 * 511);	
	JLS_ERROR error = JpegLsDecode(&rgbyteOut[0], rgbyteOut.size(), &rgbyteCompressed[0], int(rgbyteCompressed.size()), NULL);

	ASSERT(error == UncompressedBufferTooSmall);	
}


void TestBadImage()
{
	std::vector<BYTE> rgbyteCompressed;
	if (!ReadFile("test/BadCompressedStream.jls", &rgbyteCompressed, 0))
		return;

	std::vector<BYTE> rgbyteOut(2500 * 3000 * 2);
	JLS_ERROR error = JpegLsDecode(&rgbyteOut[0], rgbyteOut.size(), &rgbyteCompressed[0], int(rgbyteCompressed.size()), NULL);

	ASSERT(error == UncompressedBufferTooSmall);
}



void TestDecodeRect()
{
	std::vector<BYTE> rgbyteCompressed;
	JlsParameters info;
	if (!ScanFile("test/lena8b.jls", &rgbyteCompressed, &info))
		return;

	std::vector<BYTE> rgbyteOutFull(info.width*info.height*info.components);
	JLS_ERROR error = JpegLsDecode(&rgbyteOutFull[0], rgbyteOutFull.size(), &rgbyteCompressed[0], int(rgbyteCompressed.size()), NULL);	
	ASSERT(error == OK);

	JlsRect rect = { 128, 128, 256, 1 };
	std::vector<BYTE> rgbyteOut(rect.Width * rect.Height);
	rgbyteOut.push_back(0x1f);
	error = JpegLsDecodeRect(&rgbyteOut[0], rgbyteOut.size(), &rgbyteCompressed[0], int(rgbyteCompressed.size()), rect, NULL);	
	ASSERT(error == OK);

	ASSERT(memcmp(&rgbyteOutFull[rect.X + rect.Y*512], &rgbyteOut[0], rect.Width * rect.Height) == 0);
	ASSERT(rgbyteOut[rect.Width * rect.Height] == 0x1f);
}




void TestEncodeFromStream(const char* file, int offset, int width, int height, int bpp, int ccomponent, int ilv, size_t expectedLength)
{
	std::basic_filebuf<char> myFile; // On the stack
	myFile.open(file, mode_input);
	ASSERT(myFile.is_open());

	myFile.pubseekoff(std::streamoff(offset), std::ios_base::cur);
	ByteStreamInfo rawStreamInfo = {&myFile};

	BYTE* compressed = new BYTE[width * height * ccomponent * 2];
	JlsParameters params = JlsParameters();
	params.height = height;
	params.width = width;
	params.components = ccomponent;
	params.bitspersample= bpp;
	params.ilv = (interleavemode) ilv;
	size_t bytesWritten = 0;

	JpegLsEncodeStream(FromByteArray(compressed, width * height * ccomponent * 2), &bytesWritten, rawStreamInfo, &params);
	ASSERT(bytesWritten == expectedLength);

	delete[] compressed;
	myFile.close();
}

bool DecodeToPnm(std::istream& jlsFile, std::ostream& pnmFile)
{	
	ByteStreamInfo compressedByteStream = {jlsFile.rdbuf()};

	JlsParameters info = JlsParameters();
	JLS_ERROR err = JpegLsReadHeaderStream(compressedByteStream, &info);
	if (err != OK)
	  	return false;
 
	int maxval = (1 << info.bitspersample) - 1;
	int id = info.components == 3 ? 6 : 5;
	info.colorTransform = XFORM_BIGENDIAN;

	pnmFile << 'P' << id << ' ' << info.width << ' ' << info.height << ' '<< maxval << "   " << std::endl;
	ByteStreamInfo pnmStream = {pnmFile.rdbuf()};

	jlsFile.seekg(0);
	JpegLsDecodeStream(pnmStream, compressedByteStream, &info);
	return true;
}

std::vector<int> readPnmHeader(std::istream& pnmFile)
{
	std::vector<int> readValues;
	
	char first = (char) pnmFile.get();

	if (first != 'P')
		return readValues;

	while (readValues.size() < 4)
	{
		std::string bytes;
		std::getline(pnmFile, bytes);
		std::stringstream line(bytes);

		while (readValues.size() < 4)
		{
			int value = -1;
			line >> value;
			if (value <= 0)
				break;

			readValues.push_back(value);
		}
	}
	return readValues;
}

bool EncodePnm(std::istream& pnmFile, std::ostream& jlsFileStream)
{	
	std::vector<int> readValues = readPnmHeader(pnmFile);

	if (readValues.size() !=4)
		return false;
	
	ByteStreamInfo rawStreamInfo = {pnmFile.rdbuf()};
	ByteStreamInfo jlsStreamInfo = {jlsFileStream.rdbuf()};
	
	JlsParameters params = JlsParameters();
	int componentCount = readValues[0] == 6 ? 3 : 1;
	params.width = readValues[1];
	params.height = readValues[2];
	params.components = componentCount;
	params.bitspersample= log_2(readValues[3]+1);
	params.ilv = componentCount == 3 ? ILV_LINE : ILV_NONE;
	params.colorTransform = XFORM_BIGENDIAN;
	size_t bytesWritten = 0;

	JpegLsEncodeStream(jlsStreamInfo, &bytesWritten, rawStreamInfo, &params);
	return true;
}



void TestDecodeFromStream(const char* strNameEncoded)
{
	std::basic_filebuf<char> jlsFile; 
	jlsFile.open(strNameEncoded, mode_input);
	ASSERT(jlsFile.is_open());
	ByteStreamInfo compressedByteStream = {&jlsFile};

	JlsParameters info = JlsParameters();
	JLS_ERROR err = JpegLsReadHeaderStream(compressedByteStream, &info);

	jlsFile.pubseekpos(std::ios::beg, std::ios_base::in);

	std::basic_stringbuf<char> buf;
	ByteStreamInfo rawStreamInfo = { &buf };

	err = JpegLsDecodeStream(rawStreamInfo, compressedByteStream, NULL);
	////size_t outputCount = buf.str().size();

	ASSERT(err == OK);
	//ASSERT(outputCount == 512 * 512);
}


JLS_ERROR DecodeRaw(const char* strNameEncoded, const char* strNameOutput)
{
	std::fstream jlsFile(strNameEncoded, mode_input); 	
	ByteStreamInfo compressedByteStream = {jlsFile.rdbuf()};

	std::fstream rawFile(strNameOutput, mode_output); 
	ByteStreamInfo rawStream = {rawFile.rdbuf()};

	JLS_ERROR value = JpegLsDecodeStream(rawStream, compressedByteStream, NULL);
	jlsFile.close();
	rawFile.close();

	return value;
}

void TestEncodeFromStream()
{
	////TestDecodeFromStream("test/user_supplied/output.jls");

	TestEncodeFromStream("test/0015.RAW", 0, 1024, 1024, 8, 1,0,    0x3D3ee);
	//TestEncodeFromStream("test/MR2_UNC", 1728, 1024, 1024, 16, 1,0, 0x926e1);
	TestEncodeFromStream("test/conformance/TEST8.PPM", 15, 256, 256, 8,3,2, 99734);
	TestEncodeFromStream("test/conformance/TEST8.PPM", 15, 256, 256, 8,3,1, 100615);
	
}


void TestColorTransforms_HpImages();
void TestConformance();
void TestSampleAnnexH3();
void PerformanceTests();
void DamagedBitstreamTests();
void TestDicomWG4Images();

void UnitTest()
{
//	TestBadImage();

	printf("Test Conformance\r\n");
	TestEncodeFromStream();
	TestConformance();

	TestDecodeRect();

	printf("Test Traits\r\n");
	TestTraits16bit();
	TestTraits8bit();

	printf("Windows bitmap BGR/BGRA output\r\n");
	TestBgr();
	TestBgra();

	printf("Test Small buffer\r\n");
	TestTooSmallOutputBuffer();
	
	TestFailOnTooSmallOutputBuffer();

	printf("Test Color transform equivalence on HP images\r\n");
	TestColorTransforms_HpImages();

	printf("Test Annex H3\r\n");
	TestSampleAnnexH3();

	TestNoiseImage();
}


int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		printf("CharLS test runner.\r\nOptions: -unittest, -bitstreamdamage, -performance, -dontwait -decoderaw -encodepnm -decodetopnm \r\n");		
		return 0;
	}

	bool wait = true;
	for (int i = 1; i < argc; ++i)
	{
		std::string str = argv[i];
		if (str.compare("-unittest") == 0)
		{
			UnitTest();
			continue;
		}

		if (str.compare("-decoderaw") == 0)
		{
			if (i != 1 || argc != 4)
			{
				printf("Syntax: -decoderaw inputfile outputfile \r\n");		
				return 0;
			}
			return DecodeRaw(argv[2],argv[3]);		
		}

		if (str.compare("-decodetopnm") == 0)
		{
			if (i != 1 || argc != 4)
			{
				printf("Syntax: -decodetopnm inputfile outputfile \r\n");		
				return 0;
			}
			std::fstream pnmFile(argv[3], mode_output); 
			std::fstream jlsFile(argv[2], mode_input);

			return DecodeToPnm(jlsFile, pnmFile);			
		}

		if (str.compare("-encodepnm") == 0)
		{
			if (i != 1 || argc != 4)
			{
				printf("Syntax: -encodepnm inputfile outputfile \r\n");
				return 0;
			}
			std::fstream pnmFile(argv[2], mode_input); 
			std::fstream jlsFile(argv[3], mode_output); 
	
			return EncodePnm(pnmFile,jlsFile);				
		}

		if (str.compare("-bitstreamdamage") == 0)
		{
			DamagedBitstreamTests();
			continue;
		}

		if (str.compare("-performance") == 0)
		{
			PerformanceTests();
			continue;
		}

		if (str.compare("-dicom") == 0)
		{
			TestDicomWG4Images();
			continue;
		}

		if (str.compare("-dontwait") == 0)
		{
			wait = false;
			continue;
		}

		printf("Option not understood: %s\r\n", argv[i]);
		break;
	}

	if (wait)
	{
		char c;
		std::cin >> c;
		return 0;
	}
}
