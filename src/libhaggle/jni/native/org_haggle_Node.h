/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_haggle_Node */

#ifndef _Included_org_haggle_Node
#define _Included_org_haggle_Node
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_haggle_Node
 * Method:    nativeFree
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_haggle_Node_nativeFree
  (JNIEnv *, jobject);

/*
 * Class:     org_haggle_Node
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_haggle_Node_getName
  (JNIEnv *, jobject);

/*
 * Class:     org_haggle_Node
 * Method:    getNumInterfaces
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_haggle_Node_getNumInterfaces
  (JNIEnv *, jobject);

/*
 * Class:     org_haggle_Node
 * Method:    getInterfaceN
 * Signature: (I)Lorg/haggle/Interface;
 */
JNIEXPORT jobject JNICALL Java_org_haggle_Node_getInterfaceN
  (JNIEnv *, jobject, jint);

#ifdef __cplusplus
}
#endif
#endif
