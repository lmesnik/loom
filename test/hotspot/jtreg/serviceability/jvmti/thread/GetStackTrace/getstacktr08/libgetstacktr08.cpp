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
static jvmtiCapabilities caps;
static jvmtiEventCallbacks callbacks;
static jint result = PASSED;
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

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *env,
                        jthread thr, jmethodID method, jlocation location) {
  jvmtiError err;

  if (mid_checkPoint != method) {
    printf("ERROR: don't know where we get called from");
    result = STATUS_FAILED;
    return;
  }
  err = jvmti->ClearBreakpoint(mid_checkPoint, 0);
  if (err != JVMTI_ERROR_NONE) {
    printf("(ClearBreakpoint) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }
 // result = compare_stack_trace(jvmti, env,  testedThread, frames, NUMBER_OF_STACK_FRAMES) == JNI_TRUE ? PASSED : STATUS_FAILED;

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_SINGLE_STEP, thr);
  if (err != JVMTI_ERROR_NONE) {
    printf("Cannot enable step mode: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }
  printf(">>> stepping ...\n");

}

void JNICALL SingleStep(jvmtiEnv *jvmti_env, JNIEnv *env,
                        jthread thread, jmethodID method, jlocation location) {
  jvmtiError err;
  jclass klass;
  jvmtiClassDefinition classDef;

  if (wasFramePop == JNI_FALSE) {
    result = compare_stack_trace(jvmti, env,  testedThread, frames, NUMBER_OF_STACK_FRAMES, 1) == JNI_TRUE ? PASSED : STATUS_FAILED;

    if (!caps.can_pop_frame) {
      printf("Pop Frame is not implemented\n");
      err = jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                            JVMTI_EVENT_SINGLE_STEP, thread);
      if (err != JVMTI_ERROR_NONE) {
        printf("Cannot disable step mode: %s (%d)\n",
               TranslateError(err), err);
        result = STATUS_FAILED;
      }
      return;
    }
    printf(">>> popping frame ...\n");

    err = jvmti->PopFrame(thread);
    if (err != JVMTI_ERROR_NONE) {
      printf("(PopFrame) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
      return;
    }
    wasFramePop = JNI_TRUE;
  } else {
    err = jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                          JVMTI_EVENT_SINGLE_STEP, thread);
    if (err != JVMTI_ERROR_NONE) {
      printf("Cannot disable step mode: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
    }
    result = compare_stack_trace(jvmti, env,  testedThread, frames, NUMBER_OF_STACK_FRAMES, 2) == JNI_TRUE ? PASSED : STATUS_FAILED;


    if (!caps.can_redefine_classes) {
      printf("Redefine Classes is not implemented\n");
      return;
    }
    if (classBytes == NULL) {
      printf("ERROR: don't have any bytes");
      result = STATUS_FAILED;
      return;
    }
    err = jvmti->GetMethodDeclaringClass(method, &klass);
    if (err != JVMTI_ERROR_NONE) {
      printf("(GetMethodDeclaringClass(bp) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
      return;
    }
    printf(">>> redefining class ...\n");

    classDef.klass = klass;
    classDef.class_byte_count = env->GetArrayLength(classBytes);
    classDef.class_bytes = (unsigned char *) env->GetByteArrayElements(classBytes, NULL);
    err = jvmti->RedefineClasses(1, &classDef);
    if (err != JVMTI_ERROR_NONE) {
      printf("(RedefineClasses) unexpected error: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
      return;
    }
    env->DeleteGlobalRef(classBytes);
    classBytes = NULL;
    result = compare_stack_trace(jvmti, env,  testedThread, frames, NUMBER_OF_STACK_FRAMES, 2) == JNI_TRUE ? PASSED : STATUS_FAILED;
  }
}

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
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
Java_getstacktr08_getReady(JNIEnv *env, jclass cls, jthread thr, jbyteArray bytes) {
  jvmtiError err;
  jclass clazz;

  testedThread = env->NewGlobalRef(thr);

  classBytes = (jbyteArray) env->NewGlobalRef(bytes);

  clazz = env->GetObjectClass(thr);
  if (clazz == NULL) {
    printf("Cannot get the class of thread object\n");
    result = STATUS_FAILED;
    return;
  }

  mid_checkPoint = env->GetStaticMethodID(clazz, "checkPoint", "()V");
  if (mid_checkPoint == NULL) {
    printf("Cannot find Method ID for method \"checkPoint\"\n");
    result = STATUS_FAILED;
    return;
  }

  mid_chain4 = env->GetStaticMethodID(clazz, "chain4", "()V");
  if (mid_chain4 == NULL) {
    printf("Cannot find Method ID for method \"chain4\"\n");
    result = STATUS_FAILED;
    return;
  }

  err = jvmti->SetBreakpoint(mid_checkPoint, 0);
  if (err != JVMTI_ERROR_NONE) {
    printf("(SetBreakpoint) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_BREAKPOINT, NULL);
  if (err != JVMTI_ERROR_NONE) {
    printf("Failed to enable BREAKPOINT event: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }
}

JNIEXPORT void JNICALL
Java_getstacktr08_nativeChain(JNIEnv *env, jclass cls) {
  if (mid_chain4 != NULL) {
    env->CallStaticVoidMethod(cls, mid_chain4);
  }
  result = compare_stack_trace(jvmti, env,  testedThread, frames, NUMBER_OF_STACK_FRAMES, 3) == JNI_TRUE ? PASSED : STATUS_FAILED;
}

JNIEXPORT jint JNICALL
Java_getstacktr08_getRes(JNIEnv *env, jclass cls) {
  return result;
}

}
