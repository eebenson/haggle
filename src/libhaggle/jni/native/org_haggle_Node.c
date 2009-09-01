#include "org_haggle_Node.h"
#include "javaclass.h"

#include <libhaggle/haggle.h>
#include "common.h"
/*
 * Class:     org_haggle_Node
 * Method:    nativeFree
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_haggle_Node_nativeFree(JNIEnv *env, jobject obj)
{
        haggle_node_free((haggle_node_t *)get_native_handle(env, JCLASS_NODE, obj));
}

/*
 * Class:     org_haggle_Node
 * Method:    NodeArrayFromDataObject
 * Signature: (Lorg/haggle/DataObject;)[Lorg/haggle/Node;
 */
JNIEXPORT jobjectArray JNICALL Java_org_haggle_Node_nodeArrayFromDataObject(JNIEnv *env, jclass cls, jobject jdObj)
{
        return libhaggle_jni_dataobject_to_node_jobjectArray(env, (haggle_dobj_t *)get_native_handle(env, JCLASS_DATAOBJECT, jdObj));
}

/*
 * Class:     org_haggle_Node
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_haggle_Node_getName(JNIEnv *env, jobject obj)
{
        return (*env)->NewStringUTF(env, haggle_node_get_name((haggle_node_t *)get_native_handle(env, JCLASS_NODE, obj)));
}

/*
 * Class:     org_haggle_Node
 * Method:    getNumInterfaces
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_haggle_Node_getNumInterfaces(JNIEnv *env, jobject obj)
{
          return haggle_node_get_num_interfaces((haggle_node_t *)get_native_handle(env, JCLASS_NODE, obj));
}

/*
 * Class:     org_haggle_Node
 * Method:    getInterfaceN
 * Signature: ()Lorg/haggle/Interface;
 */
JNIEXPORT jobject JNICALL Java_org_haggle_Node_getInterfaceN(JNIEnv *env, jobject obj, jint n)
{
        
        return java_object_new(env, JCLASS_INTERFACE, haggle_interface_copy(haggle_node_get_interface_n((haggle_node_t *)get_native_handle(env, JCLASS_NODE, obj), n)));
}
