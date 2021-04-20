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

#define PASSED 0
#define STATUS_FAILED 2

static jvmtiEnv *jvmti = NULL;
static jvmtiEventCallbacks callbacks;
static jboolean wasFramePop = JNI_FALSE;
static jthread testedThread;
static jmethodID mid_checkPoint, mid_chain4;
static jbyteArray classBytes;
static frame_info frames[] = {
    {"Lgetstacktr08$TestThread;", "checkPoint", "()V"},
    {"Lgetstacktr08$TestThread;", "chain5", "()V"},
    {"Lgetstacktr08$TestThread;", "chain4", "()V"},
    {"Lgetstacktr08;", "nativeChain", "()V"},
    {"Lgetstacktr08$TestThread;", "chain3", "()V"},
    {"Lgetstacktr08$TestThread;", "chain2", "()V"},
    {"Lgetstacktr08$TestThread;", "chain1", "()V"},
    {"Lgetstacktr08$TestThread;", "run", "()V"},
};

#define NUMBER_OF_STACK_FRAMES ((int) (sizeof(frames)/sizeof(frame_info)))

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thr, jmethodID method, jlocation location) {
  if (mid_checkPoint != method) {
    jni->FatalError("ERROR: don't know where we get called from");
  }
  check_jvmti_status(jni, jvmti->ClearBreakpoint(mid_checkPoint, 0), "ClearBreakpoint failed.");

  if (!compare_stack_trace(jvmti, jni,  testedThread, frames, NUMBER_OF_STACK_FRAMES)) {
    jni->ThrowNew(jni->FindClass("java/lang/RuntimeException"), "Stacktrace differs from expected.");
  }

  set_event_notification_mode(jvmti, jni, JVMTI_ENABLE, JVMTI_EVENT_SINGLE_STEP, thr);
  printf(">>> stepping ...\n");

}

void JNICALL SingleStep(jvmtiEnv *jvmti_env, JNIEnv *jni,
                        jthread thread, jmethodID method, jlocation location) {
  jvmtiError err;
  jclass klass;
  jvmtiClassDefinition classDef;

  if (wasFramePop == JNI_FALSE) {

    if (!compare_stack_trace(jvmti, jni, testedThread, frames, NUMBER_OF_STACK_FRAMES, 1)) {
      jni->ThrowNew(jni->FindClass("java/lang/RuntimeException"), "Stacktrace differs from expected.");
    }

    printf(">>> popping frame ...\n");

    check_jvmti_status(jni, jvmti->PopFrame(thread), "PopFrame failed.");
    wasFramePop = JNI_TRUE;
  } else {
    set_event_notification_mode(jvmti, jni, JVMTI_DISABLE, JVMTI_EVENT_SINGLE_STEP, thread);
    if (!compare_stack_trace(jvmti, jni, testedThread, frames, NUMBER_OF_STACK_FRAMES, 2)) {
      jni->ThrowNew(jni->FindClass("java/lang/RuntimeException"), "Stacktrace differs from expected.");
    }


    if (classBytes == NULL) {
      jni->FatalError("ERROR: don't have any bytes");
    }

    check_jvmti_status(jni, jvmti->GetMethodDeclaringClass(method, &klass), "GetMethodDeclaringClass failed.");
    printf(">>> redefining class ...\n");

    classDef.klass = klass;
    classDef.class_byte_count = jni->GetArrayLength(classBytes);
    classDef.class_bytes = (unsigned char *) jni->GetByteArrayElements(classBytes, NULL);
    check_jvmti_status(jni, jvmti->RedefineClasses(1, &classDef), "RedefineClasses failed.");

    jni->DeleteGlobalRef(classBytes);
    classBytes = NULL;
    if (!compare_stack_trace(jvmti, jni, testedThread, frames, NUMBER_OF_STACK_FRAMES, 2)) {
      jni->ThrowNew(jni->FindClass("java/lang/RuntimeException"), "Stacktrace differs from expected.");
    }
  }
}

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jvmtiCapabilities caps;
  jvmtiError err;
  jint res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    printf("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  err = jvmti->GetPotentialCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(GetPotentialCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  err = jvmti->AddCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(AddCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  err = jvmti->GetCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(GetCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  if (caps.can_generate_breakpoint_events &&
      caps.can_generate_single_step_events) {
    callbacks.Breakpoint = &Breakpoint;
    callbacks.SingleStep = &SingleStep;
    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
      printf("(SetEventCallbacks) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      return JNI_ERR;
    }
  } else {
    printf("Warning: Breakpoint or SingleStep event is not implemented\n");
  }

  return JNI_OK;
}

JNIEXPORT void JNICALL
Java_getstacktr08_getReady(JNIEnv *jni, jclass cls, jthread thr, jbyteArray bytes) {

  testedThread = jni->NewGlobalRef(thr);

  classBytes = (jbyteArray) jni->NewGlobalRef(bytes);

  jclass clazz = jni->GetObjectClass(thr);
  mid_checkPoint = jni->GetStaticMethodID(clazz, "checkPoint", "()V");
  mid_chain4 = jni->GetStaticMethodID(clazz, "chain4", "()V");

  check_jvmti_status(jni, jvmti->SetBreakpoint(mid_checkPoint, 0), "SetBreakpoint failed.");
  set_event_notification_mode(jvmti, jni, JVMTI_ENABLE, JVMTI_EVENT_BREAKPOINT, NULL);
}

JNIEXPORT void JNICALL
Java_getstacktr08_nativeChain(JNIEnv *jni, jclass cls) {
  if (mid_chain4 != NULL) {
    jni->CallStaticVoidMethod(cls, mid_chain4);
  }
  if (!compare_stack_trace(jvmti, jni,  testedThread, frames, NUMBER_OF_STACK_FRAMES, 3)) {
    jni->ThrowNew(jni->FindClass("java/lang/RuntimeException"), "Stacktrace differs from expected.");
  }
}


}
