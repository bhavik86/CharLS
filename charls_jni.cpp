#include "charls_jni.h"
#include <iostream>
#include <fstream>

using namespace std;

JNIEXPORT jlong JNICALL Java_com_codecCentral_imageio_charls_CharlsDecoder_nativeDecode(JNIEnv* env, jobject obj,jobjectArray javaParameters)
{
	CharlsBridge* bridge = new CharlsBridge(env);
	int rc =  bridge->decode(obj, javaParameters);
	delete bridge;
	return rc;
}



JNIEXPORT jlong JNICALL Java_com_codecCentral_imageio_charls_CharlsEncoder_nativeEncode(JNIEnv* env, jobject obj,jobjectArray javaParameters)
{
	CharlsBridge* bridge = new CharlsBridge(env);
	int rc =  bridge->encode(obj, javaParameters);
	delete bridge;
	return rc;
}

CharlsBridge::CharlsBridge(JNIEnv* env)
{
	this->env = env;
	compressedBuffer = NULL;
	compressedBufferCapacity = 0;
	javaParameters=NULL;
	argc=0;
	argv=NULL;
	jba=NULL;
	jbBody=NULL;
	jia=NULL;
	jiBody=NULL;
	jsa=NULL; 
	jsBody=NULL;
	jbaCompressed=NULL;
	jbBodyCompressed=NULL;
	segmentPositions=NULL;
	bodySegmentPositions=NULL;
	segmentLengths=NULL;
	bodySegmentLengths=NULL;

}

CharlsBridge::~CharlsBridge()
{
	release();

}

int CharlsBridge::decode(jobject obj,jobjectArray javaParameters)
{
	this->javaParameters = javaParameters;
	jclass klass = env->GetObjectClass(obj);
	if (klass == 0)
	{
		fprintf(stderr,"GetObjectClass returned zero");
		return -1;
	}

	/* Preparing the transfer of the codestream from Java to C*/
	/*printf("C: before transfering codestream\n");*/
	jfieldID fid = env->GetFieldID(klass,"compressedStream", "[B");
	if ( catchAndRelease() == -1)
		return -1;

	jbaCompressed = (jbyteArray)env->GetObjectField( obj, fid);
	if ( catchAndRelease() == -1)
		return -1;

	jboolean	isCopy = 0;
	if (jbaCompressed != NULL)
	{
		compressedBufferCapacity = env->GetArrayLength( jbaCompressed);
		if ( catchAndRelease() == -1)
			return -1;

		jbBodyCompressed = env->GetByteArrayElements( jbaCompressed, &isCopy);
		if ( catchAndRelease() == -1)
			return -1;

		compressedBuffer = (char*)jbBodyCompressed;
	}
	//if we don't have a buffer, then try to get a file name
	if (!compressedBuffer )
	{
		jsize arraySize = env->GetArrayLength( javaParameters);
		if ( catchAndRelease() == -1)
			return -1;

		argc = (int) arraySize;
		argv = (const char**) new char[argc*sizeof(char*)];
		if(argv == NULL) 
		{
			fprintf(stderr,"%s:%d: MEMORY OUT\n",__FILE__,__LINE__);
			return -1;
		}
		for(int i = 0; i < argc; i++) 
		{
			argv[i] = NULL;
			jobject object = env->GetObjectArrayElement(javaParameters, i);
			if ( catchAndRelease() == -1)
				return -1;
			if (object != NULL)
			{
				argv[i] = env->GetStringUTFChars((jstring)object, &isCopy);
				if ( catchAndRelease() == -1)
					return -1;
			}
		}
	}

	int numPositions = 0;
	int numLengths = 0;
	bool hasFile = false;
	//extract file name and release env array
	if (argv && argv[0] && argv[0][0]!='\0')
	{
		hasFile = true;
		string fileName (argv[0]);

		//now check if it is segments
		/*printf("C: before transfering codestream\n");*/
		fid = env->GetFieldID( klass,"segmentPositions", "[J");
		if ( catchAndRelease() == -1)
			return -1;

		segmentPositions = (jlongArray)env->GetObjectField(obj, fid);
		if ( catchAndRelease() == -1)
			return -1;

		if (segmentPositions != NULL)
		{

			numPositions = env->GetArrayLength(segmentPositions);
			if ( catchAndRelease() == -1)
				return -1;

			bodySegmentPositions = env->GetLongArrayElements(segmentPositions, &isCopy);
			if ( catchAndRelease() == -1)
				return -1;

			fid = env->GetFieldID( klass,"segmentLengths", "[J");
			if ( catchAndRelease() == -1)
				return -1;

			segmentLengths = (jlongArray)env->GetObjectField( obj, fid);
			if ( catchAndRelease() == -1)
				return -1;

			if (segmentLengths != NULL)
			{

				numLengths = env->GetArrayLength( segmentLengths);
				if ( catchAndRelease() == -1)
					return -1;

				bodySegmentLengths = env->GetLongArrayElements(segmentLengths, &isCopy);
				if ( catchAndRelease() == -1)
					return -1;
			}
			if (numPositions == 0 || numLengths == 0 || numPositions != numLengths)
			{
				release();
				return -1;
			}
		}
	}

	if (hasFile)
	{
		ifstream myfile;
		int totalLength = 0;
		try
		{
			myfile.open (argv[0], ios::in | ios::app | ios::binary);
			if (numPositions > 0)
			{
				for (int i = 0; i < numPositions; ++i)
				{
					totalLength += bodySegmentLengths[i];
				}

				compressedBuffer = new char[totalLength];
				int dataRead = 0;
				for (int i = 0; i < numPositions; ++i)
				{
					myfile.seekg(bodySegmentPositions[i]);
					myfile.read((char*)(compressedBuffer + dataRead), bodySegmentLengths[i]);
					dataRead += bodySegmentLengths[i];
				}
			}
			else
			{
				myfile.seekg (0, myfile.end);
				totalLength = myfile.tellg();
				myfile.seekg (0, myfile.beg);
				compressedBuffer = new char[totalLength];
				myfile.read((char*)(compressedBuffer),totalLength);
			}

		}
		catch (...)
		{
			myfile.close();
			release();
			return -1;
		}

		tBufferInfo info = decodeByStreamsCommon(compressedBuffer, totalLength);
		if (!info.buffer)
			return -1;

		fid = env->GetFieldID( klass, "width", "I");
		if ( catchAndRelease() == -1)
			return -1;

		env->SetIntField(obj, fid, info.metadata.width);
		if ( catchAndRelease() == -1)
			return -1;

		fid = env->GetFieldID(klass, "height", "I");
		if ( catchAndRelease() == -1)
			return -1;

		env->SetIntField(obj, fid, info.metadata.height);
		if ( catchAndRelease() == -1)
			return -1;


		fid = env->GetFieldID(klass, "bitsPerSample", "I");
		if ( catchAndRelease() == -1)
			return -1;

		env->SetIntField(obj, fid, info.metadata.bitspersample);
		if ( catchAndRelease() == -1)
			return -1;


		int width = info.metadata.width;
		int height = info.metadata.height;
		jbyte		 *ptrBBody=NULL;
		jshort  *ptrSBody = NULL;
		jint		 *ptrIBody=NULL;
		int prec = info.metadata.bitspersample;
		int numcomps = info.metadata.components;
		
		if(numcomps == 1) /* Grey */
		{
			if(prec <= 8) 
			{
				jmethodID mid;
				mid = env->GetMethodID( klass, "alloc8", "()V");
				if ( catchAndRelease() == -1)
					return -1;

				env->CallVoidMethod(obj, mid);
				if ( catchAndRelease() == -1)
					return -1;

				fid = env->GetFieldID( klass,"image8", "[B");
				if ( catchAndRelease() == -1)
					return -1;

				jba = (jbyteArray)env->GetObjectField( obj, fid);
				if ( catchAndRelease() == -1)
					return -1;

				jbBody = env->GetByteArrayElements(jba, 0);
				if ( catchAndRelease() == -1)
					return -1;

				ptrBBody = jbBody;
				memcpy(ptrBBody, info.buffer, width*height);
			} 
			else /* prec[9:16] */
			{
				jmethodID mid;
				mid = env->GetMethodID( klass, "alloc16", "()V");
				if ( catchAndRelease() == -1)
					return -1;

				env->CallVoidMethod( obj, mid);
				if ( catchAndRelease() == -1)
					return -1;

				fid = env->GetFieldID( klass,"image16", "[S");
				if ( catchAndRelease() == -1)
					return -1;

				jsa = (jshortArray)env->GetObjectField( obj, fid);
				if ( catchAndRelease() == -1)
					return -1;

				jsBody = env->GetShortArrayElements( jsa, 0);
				if ( catchAndRelease() == -1)
					return -1;

				ptrSBody = jsBody;

				memcpy(ptrSBody, info.buffer, width*height*sizeof(short));
			}
		}	
	}
	return 0;
}


void CharlsBridge::release()
{
	/* Release the Java arguments array:*/
	if (argv)
	{
		int i;
		for(i = 0; i < argc; i++)
		{
			if ((argv)[i] != NULL)
			{
				jstring temp = (jstring)env->GetObjectArrayElement(javaParameters, i);
				env->ReleaseStringUTFChars(temp, (argv)[i]);
			}

		}
		delete argv;
		argv=NULL;
	}

			
	if (jba  && jbBody )
	{
		env->ReleaseByteArrayElements( jba, jbBody, 0);
		jba =NULL;
		jbBody = NULL;
	}
	if (jsa && jsBody )
	{
		env->ReleaseShortArrayElements( jsa, jsBody, 0);
		jsa =NULL;
		jsBody = NULL;
	}
	if (jia && jiBody)
	{
		env->ReleaseIntArrayElements( jia, jiBody, 0);
		jia =NULL;
		jiBody = NULL;
	}

	if (jbaCompressed &&  jbBodyCompressed)
	{
		env->ReleaseByteArrayElements(jbaCompressed, jbBodyCompressed, 0);
		jbaCompressed = NULL;
		jbBodyCompressed = NULL;
	}

	if (segmentPositions &&  bodySegmentPositions)
	{
		env->ReleaseLongArrayElements(segmentPositions, bodySegmentPositions, 0);
		segmentPositions = NULL;
		bodySegmentPositions = NULL;
	}

	if (segmentLengths &&  bodySegmentLengths)
	{
		env->ReleaseLongArrayElements(segmentLengths, bodySegmentLengths, 0);
		segmentLengths = NULL;
		bodySegmentLengths = NULL;
	}
}

bool CharlsBridge::catchAndRelease()
{
	if(env->ExceptionOccurred())
	{
		release();
		return true;
	}
	return false;
}


// Compress into JPEG
size_t CharlsBridge::code(char* inputData, size_t inputDataLength, size_t width, size_t height, char* outputData, size_t outputDataLength, size_t bitsPerSample, size_t numComponents)
{

	JlsParameters params = {};
/*
The fields in JlsCustomParameters do not control lossy/lossless. They
provide the possiblity to tune the JPEG-LS internals for better compression
ratios. Expect a lot of work and testing to achieve small improvements.

Lossy/lossless is controlled by the field allowedlossyerror. If you put in
0, encoding is lossless. If it is non-zero, then encoding is lossy. The
value of 3 is often suggested as a default.

The nice part about JPEG-LS encoding is that in lossy encoding, there is a
guarenteed maximum error for each pixel. So a pixel that has value 12,
encoded with a maximum lossy error of 3, may be decoded as a value between 9
and 15, but never anything else. In medical imaging this could be a useful
guarantee.

The not so nice part is that you may see striping artifacts when decoding
"non-natural" images. I haven't seen the effects myself on medical images,
but I suspect screenshots may suffer the most. Also, the bandwidth saving is
not as big as with other lossy schemes.

As for 12 bit, I am about to commit a unit test (with the sample you gave
me) that does a successful round trip encoding of 12 bit color. I did notice
that for 12 bit, the encoder fails if the unused bits are non-zero, but the
sample dit not suffer from that.
*/
    params.allowedlossyerror = 0;//Lossless ? 0 : LossyError;
    params.components = numComponents;
    // D_CLUNIE_RG3_JPLY.dcm. The famous 16bits allocated / 10 bits stored with the pixel value = 1024
    // CharLS properly encode 1024 considering it as 10bits data, so the output
    // Using bitsstored for the encoder gives a slightly better compression ratio, and is indeed the
    // right way of doing it.

    // gdcmData/PHILIPS_Gyroscan-8-MONO2-Odd_Sequence.dcm
	/*
    if( true || pf.GetPixelRepresentation() )
      {
      // gdcmData/CT_16b_signed-UsedBits13.dcm
      params.bitspersample = bitsallocated;
      }
    else
      {
      params.bitspersample = bitsstored;
      }
	  */
	params.bitspersample = bitsPerSample;
    params.height = height;
    params.width = width;

    if (numComponents == 4)
    {
		params.ilv = ILV_LINE;
    }
    else if (numComponents == 3)
    {
		params.ilv = ILV_LINE;
		params.colorTransform = COLORXFORM_HP1;
    }

    size_t cbyteCompressed;
	JLS_ERROR error = JpegLsEncode(outputData, outputDataLength, &cbyteCompressed, inputData, inputDataLength, &params);
    if( error != OK )
    {
 		return -1;
    }
    return cbyteCompressed*(params.bitspersample+7)/8;
}


int CharlsBridge::encode(jobject obj,jobjectArray javaParameters)
{
	this->javaParameters = javaParameters;
	jclass klass = env->GetObjectClass(obj);
	if (klass == 0)
	{
		fprintf(stderr,"GetObjectClass returned zero");
		return -1;
	}

	// get java parameters
	jboolean	isCopy = 0;
	jsize arraySize = env->GetArrayLength( javaParameters);
	if ( catchAndRelease() == -1)
		return -1;

	argc = (int) arraySize;
	argv = (const char**) new char[argc*sizeof(char*)];
	if(argv == NULL) 
	{
		fprintf(stderr,"%s:%d: MEMORY OUT\n",__FILE__,__LINE__);
		return -1;
	}
	for(int i = 0; i < argc; i++) 
	{
		argv[i] = NULL;
		jobject object = env->GetObjectArrayElement(javaParameters, i);
		if ( catchAndRelease() == -1)
			return -1;
		if (object != NULL)
		{
			argv[i] = env->GetStringUTFChars((jstring)object, &isCopy);
			if ( catchAndRelease() == -1)
				return -1;
		}
	}

		
	// get compressed stream buffer
	jfieldID fid = env->GetFieldID(klass,"compressedStream", "[B");
	if ( catchAndRelease() == -1)
		return -1;

	jbaCompressed = (jbyteArray)env->GetObjectField( obj, fid);
	if ( catchAndRelease() == -1)
		return -1;

	if (jbaCompressed != NULL)
	{
		compressedBufferCapacity = env->GetArrayLength( jbaCompressed);
		if ( catchAndRelease() == -1)
			return -1;

		jbBodyCompressed = env->GetByteArrayElements( jbaCompressed, &isCopy);
		if ( catchAndRelease() == -1)
			return -1;

		compressedBuffer = (char*)jbBodyCompressed;
	}

	

	// get image data 
	int w,h,bitsPerSample;
	
	/* Image width, height and depth*/
	fid = env->GetFieldID(klass,"width", "I");
	jint ji = env->GetIntField( obj, fid);
	w = ji;

	fid = env->GetFieldID( klass,"height", "I");
	ji = env->GetIntField( obj, fid);
	h = ji;
	
	fid = env->GetFieldID( klass,"bitsPerSample", "I");
	ji = env->GetIntField( obj, fid);
	bitsPerSample = ji;
	int depth = ((bitsPerSample + 7)/8)*8;
	int numcomps = 0;


	/* Read the image*/
	if (depth <=16) {
		numcomps = 1;
	} else {
		numcomps = 3;
	}

	size_t compressedLength = -1;

	if (depth == 8) {
		fid = env->GetFieldID( klass,"image8", "[B");	/* byteArray []*/
		jba = (jbyteArray)env->GetObjectField( obj, fid);
		int uncompressedLength = env->GetArrayLength( jba);
			
		jbBody = (jbyte*)env->GetPrimitiveArrayCritical( jba, &isCopy);

		compressedLength = code((char*)jbBody, uncompressedLength, w,h,  compressedBuffer, compressedBufferCapacity, depth, 1);

		env->ReleasePrimitiveArrayCritical(jba, jbBody, 0);
		jba =NULL;
		jbBody = NULL;
	} else if(depth == 16)
	{
		fid = env->GetFieldID( klass,"image16", "[S");	/* shortArray []*/
		jsa = (jshortArray)env->GetObjectField( obj, fid);
		int uncompressedLength = env->GetArrayLength( jsa);
			
		jsBody = (jshort*)env->GetPrimitiveArrayCritical( jsa, &isCopy);

		compressedLength = code((char*)jsBody, uncompressedLength*sizeof(short), w,h,  compressedBuffer, compressedBufferCapacity, depth, 1);

		env->ReleasePrimitiveArrayCritical( jsa, jsBody, 0);
		jsa =NULL;
		jsBody = NULL;
	} else if (depth == 24) 
	{
		fid = env->GetFieldID(klass,"image24", "[I");	/* intArray []*/
		jia = (jintArray)env->GetObjectField( obj, fid);
		int uncompressedLength = env->GetArrayLength( jia);

		jiBody = (jint*)env->GetPrimitiveArrayCritical( jia, &isCopy);

		compressedLength = code((char*)jiBody, uncompressedLength, w,h,  compressedBuffer, compressedBufferCapacity, depth, 3);

		env->ReleasePrimitiveArrayCritical( jia, jiBody, 0);
		jia = NULL;
		jiBody = NULL;
	}

	return compressedLength;
}

tBufferInfo CharlsBridge::decodeByStreamsCommon(char *buffer, size_t totalLen)
{
	tBufferInfo info;
    JlsParameters metadata = {};
    if (JpegLsReadHeader(buffer, totalLen, &metadata) != OK)
    {
		return info;
    }	

    // allowedlossyerror == 0 => Lossless
    bool LossyFlag = metadata.allowedlossyerror!= 0;
    JlsParameters params = {};
    JpegLsReadHeader(buffer, totalLen, &params);

	size_t outputSize = params.height *params.width * ((params.bitspersample + 7) / 8) * params.components;
	char* rgbOut = new char[outputSize];
    JLS_ERROR result = JpegLsDecode(rgbOut, outputSize, buffer, totalLen, &params);

    if (result != OK)
    {
		delete[] rgbOut;
        return info;
    }
	info.buffer = rgbOut;
	info.length = outputSize;
	info.metadata = metadata;
    return info;
}


