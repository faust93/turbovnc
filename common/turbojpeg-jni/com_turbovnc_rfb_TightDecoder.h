/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_turbovnc_rfb_TightDecoder */

#ifndef _Included_com_turbovnc_rfb_TightDecoder
#define _Included_com_turbovnc_rfb_TightDecoder
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_turbovnc_rfb_TightDecoder
 * Method:    tjInitDecompress
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_turbovnc_rfb_TightDecoder_tjInitDecompress
  (JNIEnv *, jobject);

/*
 * Class:     com_turbovnc_rfb_TightDecoder
 * Method:    tjDecompress
 * Signature: (J[BI[BIIIIIII)V
 */
JNIEXPORT void JNICALL Java_com_turbovnc_rfb_TightDecoder_tjDecompress__J_3BI_3BIIIIIII
  (JNIEnv *, jobject, jlong, jbyteArray, jint, jbyteArray, jint, jint, jint, jint, jint, jint, jint);

/*
 * Class:     com_turbovnc_rfb_TightDecoder
 * Method:    tjDecompress
 * Signature: (J[BI[IIIIIIII)V
 */
JNIEXPORT void JNICALL Java_com_turbovnc_rfb_TightDecoder_tjDecompress__J_3BI_3IIIIIIII
  (JNIEnv *, jobject, jlong, jbyteArray, jint, jintArray, jint, jint, jint, jint, jint, jint, jint);

/*
 * Class:     com_turbovnc_rfb_TightDecoder
 * Method:    tjDestroy
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_turbovnc_rfb_TightDecoder_tjDestroy
  (JNIEnv *, jobject, jlong);

#ifdef __cplusplus
}
#endif
#endif
