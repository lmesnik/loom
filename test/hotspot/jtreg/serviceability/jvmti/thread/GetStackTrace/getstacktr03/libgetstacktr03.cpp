/*
 * Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdio.h>
#include <string.h>
#include "jvmti.h"
#include "jvmti_common.h"
#include "../get_stack_trace.h"

extern "C" {

static jvmtiEnv *jvmti = NULL;
static frame_info expected_frames[] = {
    {"Ljava/lang/Object;", "wait", "()V"},
    {"Lgetstacktr03;", "dummy", "()V"},
    {"Lgetstacktr03;", "chain", "()V"},
    {"Lgetstacktr03$TestThread;", "run", "()V"},
};

#define NUMBER_OF_STACK_FRAMES ((int) (sizeof(expected_frames)/sizeof(frame_info)))

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jvmtiError err;
  jint res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    printf("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_suspend = 1;

  err = jvmti->AddCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(AddCapabilities) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }

  return JNI_OK;
}

JNIEXPORT void JNICALL
Java_getstacktr03_chain(JNIEnv *env, jclass cls) {
  jmethodID mid = env->GetStaticMethodID(cls, "dummy", "()V");
  env->CallStaticVoidMethod(cls, mid);
}

JNIEXPORT int JNICALL
Java_getstacktr03_check(JNIEnv *jni, jclass cls, jthread thread) {
  suspend_thread(jvmti, jni, thread);
  int result = compare_stack_trace(jvmti, jni, thread, expected_frames, NUMBER_OF_STACK_FRAMES);
  resume_thread(jvmti, jni, thread);
  return result;
}

}
