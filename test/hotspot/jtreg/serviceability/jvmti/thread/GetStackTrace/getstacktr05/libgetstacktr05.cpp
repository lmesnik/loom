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
static jmethodID mid;
static frame_info frames[] = {
    {"Lgetstacktr05$TestThread;", "chain4", "()V"},
    {"Lgetstacktr05$TestThread;", "chain3", "()V"},
    {"Lgetstacktr05$TestThread;", "chain2", "()V"},
    {"Lgetstacktr05$TestThread;", "chain1", "()V"},
    {"Lgetstacktr05$TestThread;", "run", "()V"},
};

#define NUMBER_OF_STACK_FRAMES ((int) (sizeof(frames)/sizeof(frame_info)))

void check(jvmtiEnv *jvmti_env, jthread thr) {
  jvmtiError err;
  jvmtiFrameInfo f[NUMBER_OF_STACK_FRAMES + 1];
  jclass callerClass;
  char *sigClass, *name, *sig, *generic;
  jint i, count;

  err = jvmti_env->GetStackTrace(thr,
                                 0, NUMBER_OF_STACK_FRAMES + 1, f, &count);
  if (err != JVMTI_ERROR_NONE) {
    printf("(GetStackTrace) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }
  if (count != NUMBER_OF_STACK_FRAMES) {
    printf("Wrong frame count, expected: %d, actual: %d\n",
           NUMBER_OF_STACK_FRAMES, count);
    result = STATUS_FAILED;
  }

  printf(">>>   frame count: %d\n", count);

  for (i = 0; i < count; i++) {
    printf(">>> checking frame#%d ...\n", i);

    err = jvmti_env->GetMethodDeclaringClass(f[i].method,
                                             &callerClass);
    if (err != JVMTI_ERROR_NONE) {
      printf("(GetMethodDeclaringClass#%d) unexpected error: %s (%d)\n",
             i, TranslateError(err), err);
      result = STATUS_FAILED;
      continue;
    }
    err = jvmti_env->GetClassSignature(callerClass,
                                       &sigClass, &generic);
    if (err != JVMTI_ERROR_NONE) {
      printf("(GetClassSignature#%d) unexpected error: %s (%d)\n",
             i, TranslateError(err), err);
      result = STATUS_FAILED;
      continue;
    }
    err = jvmti_env->GetMethodName(f[i].method,
                                   &name, &sig, &generic);
    if (err != JVMTI_ERROR_NONE) {
      printf("(GetMethodName#%d) unexpected error: %s (%d)\n",
             i, TranslateError(err), err);
      result = STATUS_FAILED;
      continue;
    }
    printf(">>>   class:  \"%s\"\n", sigClass);
    printf(">>>   method: \"%s%s\"\n", name, sig);

    if (i < NUMBER_OF_STACK_FRAMES) {
      if (sigClass == NULL || strcmp(sigClass, frames[i].cls) != 0) {
        printf("(frame#%d) wrong class sig: \"%s\", expected: \"%s\"\n",
               i, sigClass, frames[i].cls);
        result = STATUS_FAILED;
      }
      if (name == NULL || strcmp(name, frames[i].name) != 0) {
        printf("(frame#%d) wrong method name: \"%s\", expected: \"%s\"\n",
               i, name, frames[i].name);
        result = STATUS_FAILED;
      }
      if (sig == NULL || strcmp(sig, frames[i].sig) != 0) {
        printf("(frame#%d) wrong method sig: \"%s\", expected: \"%s\"\n",
               i, sig, frames[i].sig);
        result = STATUS_FAILED;
      }
    }
  }
}

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *env,
                        jthread thr, jmethodID method, jlocation location) {
  jvmtiError err;
  jint frameCount = 0;

  if (mid != method) {
    printf("ERROR: didn't know where we got called from");
    result = STATUS_FAILED;
    return;
  }

  printf(">>> (bp) checking frame count ...\n");

  err = jvmti->GetFrameCount(thr, &frameCount);
  if (err != JVMTI_ERROR_NONE) {
    printf("(GetFrameCount#bp) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }

  if (frameCount != NUMBER_OF_STACK_FRAMES + 1) {
    printf("(bp) wrong frame count, expected: %d, actual: %d\n",
           NUMBER_OF_STACK_FRAMES + 1, frameCount);
    result = STATUS_FAILED;
  }

  printf(">>> (bp)   frameCount: %d\n", frameCount);

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_SINGLE_STEP, thr);
  if (err != JVMTI_ERROR_NONE) {
    printf("Cannot enable step mode: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
  }

  printf(">>> stepping ...\n");
}

void JNICALL SingleStep(jvmtiEnv *jvmti_env, JNIEnv *jni,
                        jthread thr, jmethodID method, jlocation location) {
  result = compare_stack_trace(jvmti_env, jni, thr, frames, NUMBER_OF_STACK_FRAMES) == JNI_TRUE? PASSED : STATUS_FAILED;
  set_event_notification_mode(jvmti, jni,JVMTI_DISABLE,JVMTI_EVENT_SINGLE_STEP, thr);
}

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jvmtiError err;
  jint res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    printf("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_generate_breakpoint_events = 1;
  caps.can_generate_single_step_events = 1;

  err = jvmti->AddCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(AddCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  callbacks.Breakpoint = &Breakpoint;
  callbacks.SingleStep = &SingleStep;
  err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (err != JVMTI_ERROR_NONE) {
    printf("(SetEventCallbacks) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }


  return JNI_OK;
}

JNIEXPORT void JNICALL
Java_getstacktr05_getReady(JNIEnv *jni, jclass cls, jclass clazz) {
  mid = jni->GetMethodID(clazz, "checkPoint", "()V");
  if (mid == NULL) {
    printf("Cannot find Method ID for method checkPoint\n");
    result = STATUS_FAILED;
    return;
  }

  check_jvmti_status(jni, jvmti->SetBreakpoint(mid, 0), "SetBreakpoint failed.");
  set_event_notification_mode(jvmti, jni, JVMTI_ENABLE,JVMTI_EVENT_BREAKPOINT, NULL);

}

JNIEXPORT jint JNICALL
Java_getstacktr05_getRes(JNIEnv *env, jclass cls) {
  return result;
}

}
