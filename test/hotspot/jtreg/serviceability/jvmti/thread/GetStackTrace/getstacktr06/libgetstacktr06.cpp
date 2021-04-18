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
    {"Lgetstacktr06$TestThread;", "chain4", "()V"},
    {"Lgetstacktr06$TestThread;", "chain3", "()V"},
    {"Lgetstacktr06$TestThread;", "chain2", "()V"},
    {"Lgetstacktr06$TestThread;", "chain1", "()V"},
    {"Lgetstacktr06$TestThread;", "run", "()V"},
};

#define NUMBER_OF_STACK_FRAMES ((int) (sizeof(frames)/sizeof(frame_info)))


void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *env,
                        jthread thr, jmethodID method, jlocation location) {
  jvmtiError err;

  if (mid != method) {
    printf("ERROR: didn't know where we got called from");
    result = STATUS_FAILED;
    return;
  }

  err = jvmti->ClearBreakpoint(mid, 0);
  if (err != JVMTI_ERROR_NONE) {
    printf("(ClearBreakpoint) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }

  if (!caps.can_pop_frame) {
    printf("PopFrame is not implemented\n");
    err = jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                          JVMTI_EVENT_SINGLE_STEP, thr);
    if (err != JVMTI_ERROR_NONE) {
      printf("Cannot disable step mode: %s (%d)\n",
             TranslateError(err), err);
      result = STATUS_FAILED;
    }
    return;
  }

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_SINGLE_STEP, thr);
  if (err != JVMTI_ERROR_NONE) {
    printf("Cannot enable step mode: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }

  printf(">>> popping frame ...\n");

  err = jvmti->PopFrame(thr);
  if (err != JVMTI_ERROR_NONE) {
    printf("(PopFrame) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    result = STATUS_FAILED;
    return;
  }
}

void JNICALL SingleStep(jvmtiEnv *jvmti_env, JNIEnv *env,
                        jthread thr, jmethodID method, jlocation location) {
  set_event_notification_mode(jvmti, env, JVMTI_DISABLE,JVMTI_EVENT_SINGLE_STEP, thr);
  result = compare_stack_trace(jvmti_env, env, thr, frames, NUMBER_OF_STACK_FRAMES) == JNI_TRUE? PASSED : STATUS_FAILED;

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
  caps.can_pop_frame = 1;

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
Java_getstacktr06_getReady(JNIEnv *jni, jclass cls, jclass clazz) {
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
Java_getstacktr06_getRes(JNIEnv *env, jclass cls) {
  return result;
}

}
