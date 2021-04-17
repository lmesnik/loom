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
static jint result = PASSED;
static jmethodID mid;
static frame_info frames[] = {
    {"Lgetstacktr04$TestThread;", "checkPoint", "()V"},
    {"Lgetstacktr04$TestThread;", "chain4", "()V"},
    {"Lgetstacktr04$TestThread;", "chain3", "()V"},
    {"Lgetstacktr04$TestThread;", "chain2", "()V"},
    {"Lgetstacktr04$TestThread;", "chain1", "()V"},
    {"Lgetstacktr04$TestThread;", "run", "()V"},
};

#define NUMBER_OF_STACK_FRAMES ((int) (sizeof(frames)/sizeof(frame_info)))

void JNICALL Breakpoint(jvmtiEnv *jvmti_env, JNIEnv *jni,
                        jthread thr, jmethodID method, jlocation location) {
  if (mid != method) {
    printf("ERROR: didn't know where we got called from");
    result = STATUS_FAILED;
    return;
  }
  result = compare_stack_trace(jvmti, jni, thr, frames, NUMBER_OF_STACK_FRAMES) == JNI_TRUE? PASSED : STATUS_FAILED;
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

  err = jvmti->AddCapabilities(&caps);
  if (err != JVMTI_ERROR_NONE) {
    printf("(AddCapabilities) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }
  jvmtiEventCallbacks callbacks;
  callbacks.Breakpoint = &Breakpoint;
  err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (err != JVMTI_ERROR_NONE) {
    printf("(SetEventCallbacks) unexpected error: %s (%d)\n",
           TranslateError(err), err);
    return JNI_ERR;
  }

  return JNI_OK;
}

JNIEXPORT void JNICALL
Java_getstacktr04_getReady(JNIEnv *jni, jclass cls, jclass clazz) {
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
Java_getstacktr04_getRes(JNIEnv *env, jclass cls) {
  return result;
}

}
