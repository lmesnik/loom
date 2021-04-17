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

extern "C" {

#define PASSED 0
#define STATUS_FAILED 2

typedef struct {
  const char *cls;
  const char *name;
  const char *sig;
} frame_info;

static jvmtiEnv *jvmti = NULL;
static frame_info expected_virtual_frames[] = {
    {"LGetStackTraceCurrentThreadTest;", "check", "(Ljava/lang/Thread;)V"},
    {"LGetStackTraceCurrentThreadTest;", "dummy", "()V"},
    {"LGetStackTraceCurrentThreadTest;", "chain", "()V"},
    {"LTask;", "run", "()V"},
    {"Ljava/lang/VirtualThread;", "run", "(Ljava/lang/Runnable;)V"}
};

static frame_info expected_platform_frames[] = {
    {"LGetStackTraceCurrentThreadTest;", "check", "(Ljava/lang/Thread;)V"},
    {"LGetStackTraceCurrentThreadTest;", "dummy", "()V"},
    {"LGetStackTraceCurrentThreadTest;", "chain", "()V"},
    {"LTask;", "run", "()V"},
    {"Ljava/lang/Thread;", "run", "()V"}
};

constexpr int MAX_NUMBER_OF_STACK_FRAMES = 10;

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jint res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    printf("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  return JNI_OK;
}

JNIEXPORT void JNICALL
Java_GetStackTraceCurrentThreadTest_chain(JNIEnv *env, jclass cls) {
  jmethodID mid = env->GetStaticMethodID(cls, "dummy", "()V");
  env->CallStaticVoidMethod(cls, mid);
}

JNIEXPORT void JNICALL
Java_GetStackTraceCurrentThreadTest_check(JNIEnv *jni, jclass cls, jthread thread) {
  jint result = PASSED;
  jvmtiFrameInfo frames[MAX_NUMBER_OF_STACK_FRAMES];
  jclass caller_class;
  char *class_signature, *name, *sig, *generic;
  jint count;

  frame_info *expected_frames = jni->IsVirtualThread(thread)
      ? expected_virtual_frames
      : expected_platform_frames;
  int number_of_stack_frames = jni->IsVirtualThread(thread)
      ? ((int) (sizeof(expected_virtual_frames)/sizeof(frame_info)))
      : ((int) (sizeof(expected_platform_frames)/sizeof(frame_info)));

  check_jvmti_status(jni, jvmti->GetStackTrace(thread, 0, number_of_stack_frames, frames, &count),
                     "GetStackTrace failed.");

  if (count != number_of_stack_frames) {
    printf("Wrong number of expected_frames: %d, expected: %d\n", count, number_of_stack_frames);
    print_stack_trace(jvmti, jni, thread);
    result = STATUS_FAILED;
  }

  for (int i = count - 1; i >= 0; i--) {
    printf(">>> checking frame#%d ...\n", i);

    check_jvmti_status(jni, jvmti->GetMethodDeclaringClass(frames[i].method, &caller_class), "GetMethodDeclaringClass failed.");
    check_jvmti_status(jni, jvmti->GetClassSignature(caller_class, &class_signature, &generic), "GetClassSignature");
    check_jvmti_status(jni, jvmti->GetMethodName(frames[i].method, &name, &sig, &generic), "GetMethodName");

    printf(">>>   class:  '%s'\n", class_signature);
    printf(">>>   method: '%s%s'\n", name, sig);
    printf(">>>   %d ... done\n\n", i);

    if (i < number_of_stack_frames) {
      if (class_signature == NULL || strcmp(class_signature, expected_frames[i].cls) != 0) {
        printf("(frame#%d) wrong class sig: '%s', expected: '%s'\n", i, class_signature, expected_frames[i].cls);
        result = STATUS_FAILED;
      }
      if (name == NULL || strcmp(name, expected_frames[i].name) != 0) {
        printf("(frame#%d) wrong method name: '%s', expected: '%s'\n", i, name, expected_frames[i].name);
        result = STATUS_FAILED;
      }
      if (sig == NULL || strcmp(sig, expected_frames[i].sig) != 0) {
        printf("(frame#%d) wrong method sig: '%s', expected: '%s'\n", i, sig, expected_frames[i].sig);
        result = STATUS_FAILED;
      }
    }
  }
  if (result == STATUS_FAILED) {
    jni->FatalError("Stacktrace differs from expected.");
  }
}

}
