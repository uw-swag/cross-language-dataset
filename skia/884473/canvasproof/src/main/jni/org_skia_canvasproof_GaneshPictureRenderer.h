/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_skia_canvasproof_GaneshPictureRenderer */

#ifndef _Included_org_skia_canvasproof_GaneshPictureRenderer
#define _Included_org_skia_canvasproof_GaneshPictureRenderer
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_skia_canvasproof_GaneshPictureRenderer
 * Method:    DrawThisFrame
 * Signature: (IIFJJ)V
 */
JNIEXPORT void JNICALL Java_org_skia_canvasproof_GaneshPictureRenderer_DrawThisFrame
  (JNIEnv *, jclass, jint, jint, jfloat, jlong, jlong);

/*
 * Class:     org_skia_canvasproof_GaneshPictureRenderer
 * Method:    Ctor
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_skia_canvasproof_GaneshPictureRenderer_Ctor
  (JNIEnv *, jclass);

/*
 * Class:     org_skia_canvasproof_GaneshPictureRenderer
 * Method:    CleanUp
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_org_skia_canvasproof_GaneshPictureRenderer_CleanUp
  (JNIEnv *, jclass, jlong);

/*
 * Class:     org_skia_canvasproof_GaneshPictureRenderer
 * Method:    GetCullRect
 * Signature: (Landroid/graphics/Rect;J)V
 */
JNIEXPORT void JNICALL Java_org_skia_canvasproof_GaneshPictureRenderer_GetCullRect
  (JNIEnv *, jclass, jobject, jlong);

#ifdef __cplusplus
}
#endif
#endif
