#ifndef _Included_charls_jni_h
#define _Included_charls_jni_h

#include "interface.h"
#include <jni.h>
#include <vector>

struct tBufferInfo
{
	tBufferInfo()
	{
		buffer = NULL;
		length = 0;
	}
	char* buffer;
	size_t length;
	JlsParameters metadata;
	
};

class CharlsBridge {
  public:
    CharlsBridge(JNIEnv* env);      
	virtual ~CharlsBridge();
	int decode( jobject obj,jobjectArray javaParameters);
	int encode( jobject obj,jobjectArray javaParameters);
  private:

	tBufferInfo decodeByStreamsCommon(char *buffer, size_t totalLen);
	bool catchAndRelease();

	void release();

	JNIEnv* env;
	char *buf;
    size_t len;


	jobjectArray javaParameters;
	int argc;
	const char **argv;
	jbyteArray	jba;
	jbyte		*jbBody;
	jintArray	jia;
	jint		*jiBody;
	jshortArray jsa; 
	jshort      *jsBody;
	jbyteArray	jbaCompressed;
	jbyte		*jbBodyCompressed;
	jlongArray segmentPositions;
	jlong*     bodySegmentPositions;
	jlongArray segmentLengths;
	jlong*     bodySegmentLengths;

};

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_com_codecCentral_imageio_charls_CharlsDecoder_nativeDecode(JNIEnv *env, jobject obj,jobjectArray javaParameters);
JNIEXPORT jlong JNICALL Java_com_codecCentral_imageio_charls_CharlsEncoder_nativeEncode(JNIEnv *env, jobject obj,jobjectArray javaParameters);


#ifdef __cplusplus
}
#endif
#endif
