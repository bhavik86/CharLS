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
	buf = NULL;
	len = 0;
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
	jsize		arraySize;
	jboolean	isCopy = 0;
	jfieldID	fid;
	jclass		klass=0;
	jobject		object = NULL;
	bool hasFile = false;

		/* JNI reference to the calling class 
	*/
	klass = env->GetObjectClass(obj);
	if (klass == 0)
	{
		fprintf(stderr,"GetObjectClass returned zero");
		return -1;
	}

	/* Preparing the transfer of the codestream from Java to C*/
	/*printf("C: before transfering codestream\n");*/
	fid = (env)->GetFieldID(klass,"compressedStream", "[B");
	if ( catchAndRelease() == -1)
		return -1;

	jbaCompressed = (jbyteArray)(env)->GetObjectField( obj, fid);
	if ( catchAndRelease() == -1)
		return -1;

	if (jbaCompressed != NULL)
	{
		len = (env)->GetArrayLength( jbaCompressed);
		if ( catchAndRelease() == -1)
			return -1;

		jbBodyCompressed = (env)->GetByteArrayElements( jbaCompressed, &isCopy);
		if ( catchAndRelease() == -1)
			return -1;

		buf = (char*)jbBodyCompressed;
	}
	//if we don't have a buffer, then try to get a file name
	if (!buf )
	{
		/* Get the String[] containing the parameters, 
		*  and converts it into a char** to simulate command line arguments.
		*/
		arraySize = (env)->GetArrayLength( javaParameters);
		if ( catchAndRelease() == -1)
			return -1;

		argc = (int) arraySize;

		if(argc != 1) /* program name plus input file */
		{
			fprintf(stderr,"%s:%d: input file missing\n",__FILE__,__LINE__);
			return -1;
		}
		argv = (const char**) new char[argc*sizeof(char*)];
		if(argv == NULL) 
		{
			fprintf(stderr,"%s:%d: MEMORY OUT\n",__FILE__,__LINE__);
			return -1;
		}
		for(int i = 0; i < argc; i++) 
		{
			argv[i] = NULL;
			object = (env)->GetObjectArrayElement(javaParameters, i);
			if ( catchAndRelease() == -1)
				return -1;
			if (object != NULL)
			{
				argv[i] = (env)->GetStringUTFChars((jstring)object, &isCopy);
				if ( catchAndRelease() == -1)
					return -1;
			}

		}
	}

	int numPositions = 0;
	int numLengths = 0;
	//extract file name and release env array
	if (argv && argv[0] && argv[0][0]!='\0')
	{
		hasFile = true;
		string fileName (argv[0]);

		//now check if it is segments
		/*printf("C: before transfering codestream\n");*/
		fid = (env)->GetFieldID( klass,"segmentPositions", "[J");
		if ( catchAndRelease() == -1)
			return -1;

		segmentPositions = (jlongArray)(env)->GetObjectField(obj, fid);
		if ( catchAndRelease() == -1)
			return -1;

		if (segmentPositions != NULL)
		{

			numPositions = (env)->GetArrayLength(segmentPositions);
			if ( catchAndRelease() == -1)
				return -1;

			bodySegmentPositions = (env)->GetLongArrayElements(segmentPositions, &isCopy);
			if ( catchAndRelease() == -1)
				return -1;

			fid = (env)->GetFieldID( klass,"segmentLengths", "[J");
			if ( catchAndRelease() == -1)
				return -1;

			segmentLengths = (jlongArray)(env)->GetObjectField( obj, fid);
			if ( catchAndRelease() == -1)
				return -1;

			if (segmentLengths != NULL)
			{

				numLengths = (env)->GetArrayLength( segmentLengths);
				if ( catchAndRelease() == -1)
					return -1;

				bodySegmentLengths = (env)->GetLongArrayElements(segmentLengths, &isCopy);
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

				buf = new char[totalLength];
				int dataRead = 0;
				for (int i = 0; i < numPositions; ++i)
				{
					myfile.seekg(bodySegmentPositions[i]);
					myfile.read((char*)(buf + dataRead), bodySegmentLengths[i]);
					dataRead += bodySegmentLengths[i];
				}
			}
			else
			{
				myfile.seekg (0, myfile.end);
				totalLength = myfile.tellg();
				myfile.seekg (0, myfile.beg);
				buf = new char[totalLength];
				myfile.read((char*)(buf),totalLength);
			}

		}
		catch (...)
		{
			myfile.close();
			release();
			return -1;
		}

		tBufferInfo info = decodeByStreamsCommon(buf, totalLength);
		if (!info.buffer)
			return -1;

		fid = (env)->GetFieldID( klass, "width", "I");
		if ( catchAndRelease() == -1)
			return -1;

		(env)->SetIntField(obj, fid, info.metadata.width);
		if ( catchAndRelease() == -1)
			return -1;

		fid = (env)->GetFieldID(klass, "height", "I");
		if ( catchAndRelease() == -1)
			return -1;

		(env)->SetIntField(obj, fid, info.metadata.height);
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
				mid = (env)->GetMethodID( klass, "alloc8", "()V");
				if ( catchAndRelease() == -1)
					return -1;

				(env)->CallVoidMethod(obj, mid);
				if ( catchAndRelease() == -1)
					return -1;

				fid = (env)->GetFieldID( klass,"image8", "[B");
				if ( catchAndRelease() == -1)
					return -1;

				jba = (jbyteArray)(env)->GetObjectField( obj, fid);
				if ( catchAndRelease() == -1)
					return -1;

				jbBody = (env)->GetByteArrayElements(jba, 0);
				if ( catchAndRelease() == -1)
					return -1;

				ptrBBody = jbBody;
				memcpy(ptrBBody, info.buffer, width*height);
			} 
			else /* prec[9:16] */
			{
				jmethodID mid;
				mid = (env)->GetMethodID( klass, "alloc16", "()V");
				if ( catchAndRelease() == -1)
					return -1;

				(env)->CallVoidMethod( obj, mid);
				if ( catchAndRelease() == -1)
					return -1;

				fid = (env)->GetFieldID( klass,"image16", "[S");
				if ( catchAndRelease() == -1)
					return -1;

				jsa = (jshortArray)(env)->GetObjectField( obj, fid);
				if ( catchAndRelease() == -1)
					return -1;

				jsBody = (env)->GetShortArrayElements( jsa, 0);
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
		for(i = 1; i < argc; i++)
		{
			if ((argv)[i] != NULL)
			{
				(env)->ReleaseStringUTFChars((jstring)(env)->GetObjectArrayElement(javaParameters, i-1), (argv)[i]);
			}

		}
		delete argv;
		argv=NULL;
	}

			
	if (jba  && jbBody )
	{
		(env)->ReleaseByteArrayElements( jba, jbBody, 0);
		jba =NULL;
		jbBody = NULL;
	}
	if (jsa && jsBody )
	{
		(env)->ReleaseShortArrayElements( jsa, jsBody, 0);
		jsa =NULL;
		jsBody = NULL;
	}
	if (jia && jiBody)
	{
		(env)->ReleaseIntArrayElements( jia, jiBody, 0);
		jia =NULL;
		jiBody = NULL;
	}

	if (jbaCompressed &&  jbBodyCompressed)
	{
		(env)->ReleaseByteArrayElements(jbaCompressed, jbBodyCompressed, 0);
		jbaCompressed = NULL;
		jbBodyCompressed = NULL;
	}

	if (segmentPositions &&  bodySegmentPositions)
	{
		(env)->ReleaseLongArrayElements(segmentPositions, bodySegmentPositions, 0);
		segmentPositions = NULL;
		bodySegmentPositions = NULL;
	}

	if (segmentLengths &&  bodySegmentLengths)
	{
		(env)->ReleaseLongArrayElements(segmentLengths, bodySegmentLengths, 0);
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


int CharlsBridge::encode(jobject obj,jobjectArray javaParameters)
{

	return 0;
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


