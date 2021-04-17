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

#ifndef GET_STACK_TRACE_H
#define GET_STACK_TRACE_H
#include "jvmti.h"

typedef struct {
  const char *cls;
  const char *name;
  const char *sig;
} frame_info;


int compare_stack_trace(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, frame_info expected_frames[], int expected_frames_length) {
  int result = JNI_TRUE;
  char *class_signature, *name, *sig, *generic;
  jint count;
  const int MAX_NUMBER_OF_FRAMES = 32;
  jvmtiFrameInfo frames[MAX_NUMBER_OF_FRAMES];
  jclass caller_class;

  jvmtiError err = jvmti->GetStackTrace(thread, 0, MAX_NUMBER_OF_FRAMES, frames, &count);
  check_jvmti_status(jni, err, "GetStackTrace failed.");
  if (count < expected_frames_length) {
    printf("Number of expected_frames: %d is less then expected: %d\n",
           count, expected_frames_length);
    result = JNI_FALSE;
  }
  for (int i = 0; i < count; i++) {
    printf(">>> checking frame#%d ...\n", count - 1 - i);
    check_jvmti_status(jni, jvmti->GetMethodDeclaringClass(frames[count - 1 - i].method, &caller_class), "GetMethodDeclaringClass failed.");
    check_jvmti_status(jni, jvmti->GetClassSignature(caller_class, &class_signature, &generic), "GetClassSignature");
    check_jvmti_status(jni, jvmti->GetMethodName(frames[count - 1 - i].method, &name, &sig, &generic), "GetMethodName");

    printf(">>>   class:  \"%s\"\n", class_signature);
    printf(">>>   method: \"%s%s\"\n", name, sig);
    printf(">>>   %d ... done\n", i);

    if (i < expected_frames_length) {
      if (class_signature == NULL || strcmp(class_signature, expected_frames[expected_frames_length - 1 - i].cls) != 0) {
        printf("(frame#%d) wrong class sig: \"%s\", expected: \"%s\"\n",
               expected_frames_length - 1 - i, class_signature, expected_frames[expected_frames_length - 1 - i].cls);
        result = JNI_FALSE;
      }
      if (name == NULL || strcmp(name, expected_frames[expected_frames_length - 1 - i].name) != 0) {
        printf("(frame#%d) wrong method name: \"%s\", expected: \"%s\"\n",
               expected_frames_length - 1 - i, name, expected_frames[expected_frames_length - 1 - i].name);
        result = JNI_FALSE;
      }
      if (sig == NULL || strcmp(sig, expected_frames[expected_frames_length - 1 - i].sig) != 0) {
        printf("(frame#%d) wrong method sig: \"%s\", expected: \"%s\"\n",
               expected_frames_length - 1 - i, sig, expected_frames[expected_frames_length - 1 - i].sig);
        result = JNI_FALSE;
      }
    }
  }
  return result;
}



#endif //GET_STACK_TRACE_H
