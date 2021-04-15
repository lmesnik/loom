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

#ifndef JVMTI_COMMON_H
#define JVMTI_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define NSK_TRUE 1
#define NSK_FALSE 0

#define NSK_DISPLAY0 printf
#define NSK_DISPLAY1 printf
#define NSK_DISPLAY2 printf
#define NSK_DISPLAY3 printf


#define NSK_COMPLAIN0 printf
#define NSK_COMPLAIN1 printf
#define NSK_COMPLAIN3 printf

const char* TranslateState(jint flags);
const char* TranslateError(jvmtiError err);

static jvmtiExtensionFunction GetVirtualThread_func = NULL;
static jvmtiExtensionFunction GetCarrierThread_func = NULL;

char*
jlong_to_string(jlong value, char *string) {
  char buffer[32];
  char *pbuf, *pstr;

  pstr = string;
  if (value == 0) {
    *pstr++ = '0';
  } else {
    if (value < 0) {
      *pstr++ = '-';
      value = -value;
    }
    pbuf = buffer;
    while (value != 0) {
      *pbuf++ = '0' + (char)(value % 10);
      value = value / 10;
    }
    while (pbuf != buffer) {
      *pstr++ = *--pbuf;
    }
  }
  *pstr = '\0';

  return string;
}

static void
fatal(JNIEnv* jni, const char* msg) {
  jni->FatalError(msg);
  fflush(stdout);
}


static void
check_jvmti_status(JNIEnv* jni, jvmtiError err, const char* msg) {
  if (err != JVMTI_ERROR_NONE) {
    printf("check_jvmti_status: JVMTI function returned error: %s (%d)\n", TranslateError(err), err);
    jni->FatalError(msg);
  }
}

/* JVMTI helper wrappers. Check errors and fail or return null if jvmti operation failed. */

// Monitors often created in Agent_Initialize(..) where JNIEnv* jni doesn't exist.
jrawMonitorID
create_raw_monitor(jvmtiEnv *jvmti, const char* name) {
  jrawMonitorID lock;
  jvmtiError err;
  err = jvmti->CreateRawMonitor(name, &lock);
  if (err != JVMTI_ERROR_NONE) {
    return nullptr;
  }
  return lock;
}

class RawMonitorLocker {
 private:
  jvmtiEnv* _jvmti;
  JNIEnv* _jni;
  jrawMonitorID _monitor;
 public:
  RawMonitorLocker(jvmtiEnv *jvmti,JNIEnv* jni, jrawMonitorID monitor):_jvmti(jvmti), _jni(jni), _monitor(monitor) {
    check_jvmti_status(_jni, _jvmti->RawMonitorEnter(_monitor), "Fatal Error in RawMonitorEnter.");
  }
  ~RawMonitorLocker() {
    check_jvmti_status(_jni, _jvmti->RawMonitorExit(_monitor), "Fatal Error in RawMonitorEnter.");

  }

  void wait(jlong millis) {
    check_jvmti_status(_jni, _jvmti->RawMonitorWait(_monitor, millis), "Fatal Error in RawMonitorWait.");
  }

  void wait() {
    wait(0);
  }

  void notify() {
    check_jvmti_status(_jni, _jvmti->RawMonitorNotify(_monitor), "Fatal Error in RawMonitorNotify.");
  }

  void notify_all() {
    check_jvmti_status(_jni, _jvmti->RawMonitorNotifyAll(_monitor), "Fatal Error in RawMonitorNotifyAll.");
  }

};


static void
deallocate(jvmtiEnv *jvmti, JNIEnv* jni, void* ptr) {
  jvmtiError err;

  err = jvmti->Deallocate((unsigned char*)ptr);
  check_jvmti_status(jni, err, "deallocate: error in JVMTI Deallocate call");
}


static char*
get_method_class_name(jvmtiEnv *jvmti, JNIEnv* jni, jmethodID method) {
  jclass klass = NULL;
  char*  cname = NULL;
  char*  result = NULL;
  jvmtiError err;

  err = jvmti->GetMethodDeclaringClass(method, &klass);
  check_jvmti_status(jni, err, "get_method_class_name: error in JVMTI GetMethodDeclaringClass");

  err = jvmti->GetClassSignature(klass, &cname, NULL);
  check_jvmti_status(jni, err, "get_method_class_name: error in JVMTI GetClassSignature");

  size_t len = strlen(cname) - 2; // get rid of leading 'L' and trailing ';'

  err = jvmti->Allocate((jlong)(len + 1), (unsigned char**)&result);
  check_jvmti_status(jni, err, "get_method_class_name: error in JVMTI Allocate");

  strncpy(result, cname + 1, len); // skip leading 'L'
  result[len] = '\0';
  return result;
}


static void
print_method(jvmtiEnv *jvmti, JNIEnv* jni, jmethodID method, jint depth) {
  char*  cname = NULL;
  char*  mname = NULL;
  char*  msign = NULL;
  jvmtiError err;

  cname = get_method_class_name(jvmti, jni, method);

  err = jvmti->GetMethodName(method, &mname, &msign, NULL);
  check_jvmti_status(jni, err, "print_method: error in JVMTI GetMethodName");

  printf("%2d: %s: %s%s\n", depth, cname, mname, msign);
  fflush(0);
}

void
print_thread_info(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread_obj) {
  jvmtiThreadInfo thread_info;
  jint thread_state;
  check_jvmti_status(jni, jvmti->GetThreadInfo(thread_obj, &thread_info), "Error in GetThreadInfo");
  check_jvmti_status(jni, jvmti->GetThreadState(thread_obj, &thread_state), "Error in GetThreadInfo");
  const char* state = TranslateState(thread_state);
  printf("Thread: %p, name: %s, state: %s, attrs: %s %s\n", thread_obj, thread_info.name, TranslateState(thread_state),
         (jni->IsVirtualThread(thread_obj)? "virtual": "kernel"), (thread_info.is_daemon ? "daemon": ""));
}

static void
print_stack_trace_frames(jvmtiEnv *jvmti, JNIEnv *jni, jint count, jvmtiFrameInfo *frames) {
  printf("JVMTI Stack Trace: frame count: %d\n", count);
  for (int depth = 0; depth < count; depth++) {
    print_method(jvmti, jni, frames[depth].method, depth);
  }
  printf("\n");
}

static jint
get_frame_count(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
  jint frame_count;
  jvmtiError err = jvmti->GetFrameCount(thread, &frame_count);
  check_jvmti_status(jni, err, "get_frame_count: error in JVMTI GetFrameCount call");
  return frame_count;
}

static char*
get_thread_name(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
  jvmtiThreadInfo thr_info;
  jvmtiError err;

  memset(&thr_info, 0, sizeof(thr_info));
  err = jvmti->GetThreadInfo(thread, &thr_info);
  check_jvmti_status(jni, err, "get_thread_name: error in JVMTI GetThreadInfo call");

  return thr_info.name == NULL ? (char*)"<Unnamed thread>" : thr_info.name;
}

static char*
get_method_name(jvmtiEnv *jvmti, JNIEnv* jni, jmethodID method) {
  char*  mname = NULL;
  jvmtiError err;

  err = jvmti->GetMethodName(method, &mname, NULL, NULL);
  check_jvmti_status(jni, err, "get_method_name: error in JVMTI GetMethodName call");

  return mname;
}


static jclass
find_class(jvmtiEnv *jvmti, JNIEnv *jni, jobject loader, const char* cname) {
  jclass *classes = NULL;
  jint count = 0;
  jvmtiError err;

  err = jvmti->GetClassLoaderClasses(loader, &count, &classes);
  check_jvmti_status(jni, err, "find_class: error in JVMTI GetClassLoaderClasses");

  // Find the jmethodID of the specified method
  while (--count >= 0) {
    char* name = NULL;
    jclass klass = classes[count];

    err = jvmti->GetClassSignature(klass, &name, NULL);
    check_jvmti_status(jni, err, "find_class: error in JVMTI GetClassSignature call");

    bool found = (strcmp(name, cname) == 0);
    deallocate(jvmti, jni, (void*)name);
    if (found) {
      return klass;
    }
  }
  return NULL;
}

static jmethodID
find_method(jvmtiEnv *jvmti, JNIEnv *jni, jclass klass, const char* mname) {
  jmethodID *methods = NULL;
  jmethodID method = NULL;
  jint count = 0;
  jvmtiError err;

  err = jvmti->GetClassMethods(klass, &count, &methods);
  check_jvmti_status(jni, err, "find_method: error in JVMTI GetClassMethods");

  // Find the jmethodID of the specified method
  while (--count >= 0) {
    char* name = NULL;

    jmethodID meth = methods[count];

    err = jvmti->GetMethodName(meth, &name, NULL, NULL);
    check_jvmti_status(jni, err, "find_method: error in JVMTI GetMethodName call");

    bool found = (strcmp(name, mname) == 0);
    deallocate(jvmti, jni, (void*)name);
    if (found) {
      method = meth;
      break;
    }
  }
  deallocate(jvmti, jni, (void*)methods);
  return method;
}

#define MAX_FRAME_COUNT_PRINT_STACK_TRACE 200

static void
print_current_stack_trace(jvmtiEnv *jvmti, JNIEnv* jni) {
  jvmtiFrameInfo frames[MAX_FRAME_COUNT_PRINT_STACK_TRACE];
  jint count = 0;
  jvmtiError err;

  err = jvmti->GetStackTrace(NULL, 0, MAX_FRAME_COUNT_PRINT_STACK_TRACE, frames, &count);
  check_jvmti_status(jni, err, "print_stack_trace: error in JVMTI GetStackTrace");

  printf("JVMTI Stack Trace for current thread: frame count: %d\n", count);
  for (int depth = 0; depth < count; depth++) {
    print_method(jvmti, jni, frames[depth].method, depth);
  }
  printf("\n");
}

static void
print_stack_trace(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
  jvmtiFrameInfo frames[MAX_FRAME_COUNT_PRINT_STACK_TRACE];
  char* tname = get_thread_name(jvmti, jni, thread);
  jint count = 0;
  jvmtiError err;

  err = jvmti->GetStackTrace(thread, 0, MAX_FRAME_COUNT_PRINT_STACK_TRACE, frames, &count);
  check_jvmti_status(jni, err, "print_stack_trace: error in JVMTI GetStackTrace");

  printf("JVMTI Stack Trace for thread %s: frame count: %d\n", tname, count);
  for (int depth = 0; depth < count; depth++) {
    print_method(jvmti, jni, frames[depth].method, depth);
  }
  deallocate(jvmti, jni, (void*)tname);
  printf("\n");
}


static void suspend_thread(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
  check_jvmti_status(jni, jvmti->SuspendThread(thread), "error in JVMTI SuspendThread");
}

static void resume_thread(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
  check_jvmti_status(jni, jvmti->ResumeThread(thread), "error in JVMTI ResumeThread");
}

static jthread get_current_thread(jvmtiEnv *jvmti, JNIEnv* jni) {
  jthread thread;
  check_jvmti_status(jni, jvmti->GetCurrentThread(&thread), "error in JVMTI ResumeThread");
  return thread;
}




/* Commonly used helper functions */
const char*
TranslateState(jint flags) {
  static char str[15 * 20];

  if (flags == 0) {
    return "<none>";
  }
  str[0] = '\0';

  if (flags & JVMTI_THREAD_STATE_ALIVE) {
    strcat(str, " ALIVE");
  }
  if (flags & JVMTI_THREAD_STATE_TERMINATED) {
    strcat(str, " TERMINATED");
  }
  if (flags & JVMTI_THREAD_STATE_RUNNABLE) {
    strcat(str, " RUNNABLE");
  }
  if (flags & JVMTI_THREAD_STATE_WAITING) {
    strcat(str, " WAITING");
  }
  if (flags & JVMTI_THREAD_STATE_WAITING_INDEFINITELY) {
    strcat(str, " WAITING_INDEFINITELY");
  }
  if (flags & JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT) {
    strcat(str, " WAITING_WITH_TIMEOUT");
  }
  if (flags & JVMTI_THREAD_STATE_SLEEPING) {
    strcat(str, " SLEEPING");
  }
  if (flags & JVMTI_THREAD_STATE_IN_OBJECT_WAIT) {
    strcat(str, " IN_OBJECT_WAIT");
  }
  if (flags & JVMTI_THREAD_STATE_PARKED) {
    strcat(str, " PARKED");
  }
  if (flags & JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER) {
    strcat(str, " BLOCKED_ON_MONITOR_ENTER");
  }
  if (flags & JVMTI_THREAD_STATE_SUSPENDED) {
    strcat(str, " SUSPENDED");
  }
  if (flags & JVMTI_THREAD_STATE_INTERRUPTED) {
    strcat(str, " INTERRUPTED");
  }
  if (flags & JVMTI_THREAD_STATE_IN_NATIVE) {
    strcat(str, " IN_NATIVE");
  }
  return str;
}

const char*
TranslateEvent(jvmtiEvent event_type) {
    switch (event_type) {
    case JVMTI_EVENT_VM_INIT:
        return ("JVMTI_EVENT_VM_INIT");
    case JVMTI_EVENT_VM_DEATH:
        return ("JVMTI_EVENT_VM_DEATH");
    case JVMTI_EVENT_THREAD_START:
        return ("JVMTI_EVENT_THREAD_START");
    case JVMTI_EVENT_THREAD_END:
        return ("JVMTI_EVENT_THREAD_END");
    case JVMTI_EVENT_CLASS_FILE_LOAD_HOOK:
        return ("JVMTI_EVENT_CLASS_FILE_LOAD_HOOK");
    case JVMTI_EVENT_CLASS_LOAD:
        return ("JVMTI_EVENT_CLASS_LOAD");
    case JVMTI_EVENT_CLASS_PREPARE:
        return ("JVMTI_EVENT_CLASS_PREPARE");
    case JVMTI_EVENT_VM_START:
        return ("JVMTI_EVENT_VM_START");
    case JVMTI_EVENT_EXCEPTION:
        return ("JVMTI_EVENT_EXCEPTION");
    case JVMTI_EVENT_EXCEPTION_CATCH:
        return ("JVMTI_EVENT_EXCEPTION_CATCH");
    case JVMTI_EVENT_SINGLE_STEP:
        return ("JVMTI_EVENT_SINGLE_STEP");
    case JVMTI_EVENT_FRAME_POP:
        return ("JVMTI_EVENT_FRAME_POP");
    case JVMTI_EVENT_BREAKPOINT:
        return ("JVMTI_EVENT_BREAKPOINT");
    case JVMTI_EVENT_FIELD_ACCESS:
        return ("JVMTI_EVENT_FIELD_ACCESS");
    case JVMTI_EVENT_FIELD_MODIFICATION:
        return ("JVMTI_EVENT_FIELD_MODIFICATION");
    case JVMTI_EVENT_METHOD_ENTRY:
        return ("JVMTI_EVENT_METHOD_ENTRY");
    case JVMTI_EVENT_METHOD_EXIT:
        return ("JVMTI_EVENT_METHOD_EXIT");
    case JVMTI_EVENT_NATIVE_METHOD_BIND:
        return ("JVMTI_EVENT_NATIVE_METHOD_BIND");
    case JVMTI_EVENT_COMPILED_METHOD_LOAD:
        return ("JVMTI_EVENT_COMPILED_METHOD_LOAD");
    case JVMTI_EVENT_COMPILED_METHOD_UNLOAD:
        return ("JVMTI_EVENT_COMPILED_METHOD_UNLOAD");
    case JVMTI_EVENT_DYNAMIC_CODE_GENERATED:
        return ("JVMTI_EVENT_DYNAMIC_CODE_GENERATED");
    case JVMTI_EVENT_DATA_DUMP_REQUEST:
        return ("JVMTI_EVENT_DATA_DUMP_REQUEST");
    case JVMTI_EVENT_MONITOR_WAIT:
        return ("JVMTI_EVENT_MONITOR_WAIT");
    case JVMTI_EVENT_MONITOR_WAITED:
        return ("JVMTI_EVENT_MONITOR_WAITED");
    case JVMTI_EVENT_MONITOR_CONTENDED_ENTER:
        return ("JVMTI_EVENT_MONITOR_CONTENDED_ENTER");
    case JVMTI_EVENT_MONITOR_CONTENDED_ENTERED:
        return ("JVMTI_EVENT_MONITOR_CONTENDED_ENTERED");
    case JVMTI_EVENT_GARBAGE_COLLECTION_START:
        return ("JVMTI_EVENT_GARBAGE_COLLECTION_START");
    case JVMTI_EVENT_GARBAGE_COLLECTION_FINISH:
        return ("JVMTI_EVENT_GARBAGE_COLLECTION_FINISH");
    case JVMTI_EVENT_OBJECT_FREE:
        return ("JVMTI_EVENT_OBJECT_FREE");
    case JVMTI_EVENT_VM_OBJECT_ALLOC:
        return ("JVMTI_EVENT_VM_OBJECT_ALLOC");
    default:
        return ("<unknown event>");
    }
}

const char*
TranslateError(jvmtiError err) {
    switch (err) {
    case JVMTI_ERROR_NONE:
        return ("JVMTI_ERROR_NONE");
    case JVMTI_ERROR_INVALID_THREAD:
        return ("JVMTI_ERROR_INVALID_THREAD");
    case JVMTI_ERROR_INVALID_THREAD_GROUP:
        return ("JVMTI_ERROR_INVALID_THREAD_GROUP");
    case JVMTI_ERROR_INVALID_PRIORITY:
        return ("JVMTI_ERROR_INVALID_PRIORITY");
    case JVMTI_ERROR_THREAD_NOT_SUSPENDED:
        return ("JVMTI_ERROR_THREAD_NOT_SUSPENDED");
    case JVMTI_ERROR_THREAD_SUSPENDED:
        return ("JVMTI_ERROR_THREAD_SUSPENDED");
    case JVMTI_ERROR_THREAD_NOT_ALIVE:
        return ("JVMTI_ERROR_THREAD_NOT_ALIVE");
    case JVMTI_ERROR_INVALID_OBJECT:
        return ("JVMTI_ERROR_INVALID_OBJECT");
    case JVMTI_ERROR_INVALID_CLASS:
        return ("JVMTI_ERROR_INVALID_CLASS");
    case JVMTI_ERROR_CLASS_NOT_PREPARED:
        return ("JVMTI_ERROR_CLASS_NOT_PREPARED");
    case JVMTI_ERROR_INVALID_METHODID:
        return ("JVMTI_ERROR_INVALID_METHODID");
    case JVMTI_ERROR_INVALID_LOCATION:
        return ("JVMTI_ERROR_INVALID_LOCATION");
    case JVMTI_ERROR_INVALID_FIELDID:
        return ("JVMTI_ERROR_INVALID_FIELDID");
    case JVMTI_ERROR_NO_MORE_FRAMES:
        return ("JVMTI_ERROR_NO_MORE_FRAMES");
    case JVMTI_ERROR_OPAQUE_FRAME:
        return ("JVMTI_ERROR_OPAQUE_FRAME");
    case JVMTI_ERROR_TYPE_MISMATCH:
        return ("JVMTI_ERROR_TYPE_MISMATCH");
    case JVMTI_ERROR_INVALID_SLOT:
        return ("JVMTI_ERROR_INVALID_SLOT");
    case JVMTI_ERROR_DUPLICATE:
        return ("JVMTI_ERROR_DUPLICATE");
    case JVMTI_ERROR_NOT_FOUND:
        return ("JVMTI_ERROR_NOT_FOUND");
    case JVMTI_ERROR_INVALID_MONITOR:
        return ("JVMTI_ERROR_INVALID_MONITOR");
    case JVMTI_ERROR_NOT_MONITOR_OWNER:
        return ("JVMTI_ERROR_NOT_MONITOR_OWNER");
    case JVMTI_ERROR_INTERRUPT:
        return ("JVMTI_ERROR_INTERRUPT");
    case JVMTI_ERROR_INVALID_CLASS_FORMAT:
        return ("JVMTI_ERROR_INVALID_CLASS_FORMAT");
    case JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION:
        return ("JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION");
    case JVMTI_ERROR_FAILS_VERIFICATION:
        return ("JVMTI_ERROR_FAILS_VERIFICATION");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED");
    case JVMTI_ERROR_INVALID_TYPESTATE:
        return ("JVMTI_ERROR_INVALID_TYPESTATE");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED");
    case JVMTI_ERROR_UNSUPPORTED_VERSION:
        return ("JVMTI_ERROR_UNSUPPORTED_VERSION");
    case JVMTI_ERROR_NAMES_DONT_MATCH:
        return ("JVMTI_ERROR_NAMES_DONT_MATCH");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED");
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED:
        return ("JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED");
    case JVMTI_ERROR_UNMODIFIABLE_CLASS:
        return ("JVMTI_ERROR_UNMODIFIABLE_CLASS");
    case JVMTI_ERROR_NOT_AVAILABLE:
        return ("JVMTI_ERROR_NOT_AVAILABLE");
    case JVMTI_ERROR_MUST_POSSESS_CAPABILITY:
        return ("JVMTI_ERROR_MUST_POSSESS_CAPABILITY");
    case JVMTI_ERROR_NULL_POINTER:
        return ("JVMTI_ERROR_NULL_POINTER");
    case JVMTI_ERROR_ABSENT_INFORMATION:
        return ("JVMTI_ERROR_ABSENT_INFORMATION");
    case JVMTI_ERROR_INVALID_EVENT_TYPE:
        return ("JVMTI_ERROR_INVALID_EVENT_TYPE");
    case JVMTI_ERROR_ILLEGAL_ARGUMENT:
        return ("JVMTI_ERROR_ILLEGAL_ARGUMENT");
    case JVMTI_ERROR_NATIVE_METHOD:
        return ("JVMTI_ERROR_NATIVE_METHOD");
    case JVMTI_ERROR_OUT_OF_MEMORY:
        return ("JVMTI_ERROR_OUT_OF_MEMORY");
    case JVMTI_ERROR_ACCESS_DENIED:
        return ("JVMTI_ERROR_ACCESS_DENIED");
    case JVMTI_ERROR_WRONG_PHASE:
        return ("JVMTI_ERROR_WRONG_PHASE");
    case JVMTI_ERROR_INTERNAL:
        return ("JVMTI_ERROR_INTERNAL");
    case JVMTI_ERROR_UNATTACHED_THREAD:
        return ("JVMTI_ERROR_UNATTACHED_THREAD");
    case JVMTI_ERROR_INVALID_ENVIRONMENT:
        return ("JVMTI_ERROR_INVALID_ENVIRONMENT");
    default:
        return ("<unknown error>");
    }
}

const char*
TranslatePhase(jvmtiPhase phase) {
    switch (phase) {
    case JVMTI_PHASE_ONLOAD:
        return ("JVMTI_PHASE_ONLOAD");
    case JVMTI_PHASE_PRIMORDIAL:
        return ("JVMTI_PHASE_PRIMORDIAL");
    case JVMTI_PHASE_START:
        return ("JVMTI_PHASE_START");
    case JVMTI_PHASE_LIVE:
        return ("JVMTI_PHASE_LIVE");
    case JVMTI_PHASE_DEAD:
        return ("JVMTI_PHASE_DEAD");
    default:
        return ("<unknown phase>");
    }
}

const char*
TranslateRootKind(jvmtiHeapRootKind root) {
    switch (root) {
    case JVMTI_HEAP_ROOT_JNI_GLOBAL:
        return ("JVMTI_HEAP_ROOT_JNI_GLOBAL");
    case JVMTI_HEAP_ROOT_JNI_LOCAL:
        return ("JVMTI_HEAP_ROOT_JNI_LOCAL");
    case JVMTI_HEAP_ROOT_SYSTEM_CLASS:
        return ("JVMTI_HEAP_ROOT_SYSTEM_CLASS");
    case JVMTI_HEAP_ROOT_MONITOR:
        return ("JVMTI_HEAP_ROOT_MONITOR");
    case JVMTI_HEAP_ROOT_STACK_LOCAL:
        return ("JVMTI_HEAP_ROOT_STACK_LOCAL");
    case JVMTI_HEAP_ROOT_THREAD:
        return ("JVMTI_HEAP_ROOT_THREAD");
    case JVMTI_HEAP_ROOT_OTHER:
        return ("JVMTI_HEAP_ROOT_OTHER");
    default:
        return ("<unknown root kind>");
    }
}

const char*
TranslateObjectRefKind(jvmtiObjectReferenceKind ref) {
    switch (ref) {
    case JVMTI_REFERENCE_CLASS:
        return ("JVMTI_REFERENCE_CLASS");
    case JVMTI_REFERENCE_FIELD:
        return ("JVMTI_REFERENCE_FIELD");
    case JVMTI_REFERENCE_ARRAY_ELEMENT:
        return ("JVMTI_REFERENCE_ARRAY_ELEMENT");
    case JVMTI_REFERENCE_CLASS_LOADER:
        return ("JVMTI_REFERENCE_CLASS_LOADER");
    case JVMTI_REFERENCE_SIGNERS:
        return ("JVMTI_REFERENCE_SIGNERS");
    case JVMTI_REFERENCE_PROTECTION_DOMAIN:
        return ("JVMTI_REFERENCE_PROTECTION_DOMAIN");
    case JVMTI_REFERENCE_INTERFACE:
        return ("JVMTI_REFERENCE_INTERFACE");
    case JVMTI_REFERENCE_STATIC_FIELD:
        return ("JVMTI_REFERENCE_STATIC_FIELD");
    case JVMTI_REFERENCE_CONSTANT_POOL:
        return ("JVMTI_REFERENCE_CONSTANT_POOL");
    default:
        return ("<unknown reference kind>");
    }
}

int
isThreadExpected(jvmtiEnv *jvmti, jthread thread) {
  static const char *vm_jfr_buffer_thread_name = "VM JFR Buffer Thread";
  static const char *jfr_request_timer_thread_name = "JFR request timer";
  static const char *graal_management_bean_registration_thread_name =
                        "HotSpotGraalManagement Bean Registration";
  static const char *graal_compiler_thread_name_prefix = "JVMCI CompilerThread";
  static const size_t prefixLength = strlen(graal_compiler_thread_name_prefix);

  jvmtiThreadInfo threadinfo;
  jvmtiError err = jvmti->GetThreadInfo(thread, &threadinfo);
  if (err != JVMTI_ERROR_NONE) {
    return 0;
  }
  if (strcmp(threadinfo.name, vm_jfr_buffer_thread_name) == 0) {
    return 0;
  }
  if (strcmp(threadinfo.name, jfr_request_timer_thread_name) == 0) {
    return 0;
  }
  if (strcmp(threadinfo.name, graal_management_bean_registration_thread_name) == 0)
    return 0;

  if ((strlen(threadinfo.name) > prefixLength) &&
      strncmp(threadinfo.name, graal_compiler_thread_name_prefix, prefixLength) == 0) {
    return 0;
  }
  return 1;
}

jthread
nsk_jvmti_threadByName(jvmtiEnv* jvmti, JNIEnv* jni, const char name[]) {
  jthread* threads = NULL;
  jint count = 0;
  jthread foundThread = NULL;
  int i;

  if (name == NULL) {
    return NULL;
  }

  check_jvmti_status(jni, jvmti->GetAllThreads(&count, &threads), "");

  for (i = 0; i < count; i++) {
    jvmtiThreadInfo info;

    check_jvmti_status(jni, jvmti->GetThreadInfo(threads[i], &info), "");

    if (info.name != NULL && strcmp(name, info.name) == 0) {
      foundThread = threads[i];
      break;
    }
  }

  check_jvmti_status(jni, jvmti->Deallocate((unsigned char*)threads), "");

  foundThread = (jthread) jni->NewGlobalRef(foundThread);
  return foundThread;
}

/*
 * JVMTI Extension Mechanism
 */

static const jvmtiEvent
  EXT_EVENT_VIRTUAL_THREAD_MOUNT   = (jvmtiEvent)((int)JVMTI_MIN_EVENT_TYPE_VAL - 2),
  EXT_EVENT_VIRTUAL_THREAD_UNMOUNT = (jvmtiEvent)((int)JVMTI_MIN_EVENT_TYPE_VAL - 3);

static jvmtiExtensionFunction
find_ext_function(jvmtiEnv* jvmti, JNIEnv* jni, const char* fname) {
  jint extCount = 0;
  jvmtiExtensionFunctionInfo* extList = NULL;

  jvmtiError err = jvmti->GetExtensionFunctions(&extCount, &extList);
  check_jvmti_status(jni, err, "jvmti_common find_ext_function: Error in JVMTI GetExtensionFunctions");

  for (int i = 0; i < extCount; i++) {
    if (strstr(extList[i].id, (char*)fname) != NULL) {
      return extList[i].func;
    }
  }
  return NULL;
}

static jvmtiError
GetVirtualThread(jvmtiEnv* jvmti, JNIEnv* jni, jthread cthread, jthread* vthread_ptr) {
  if (GetVirtualThread_func == NULL) { // lazily initialize function pointer
    GetVirtualThread_func = find_ext_function(jvmti, jni, "GetVirtualThread");
  }
  jvmtiError err = (*GetVirtualThread_func)(jvmti, cthread, vthread_ptr);

  return err;
}

static jvmtiError
GetCarrierThread(jvmtiEnv* jvmti, JNIEnv* jni, jthread vthread, jthread* cthread_ptr) {
  if (GetCarrierThread_func == NULL) { // lazily initialize function pointer
    GetCarrierThread_func = find_ext_function(jvmti, jni, "GetCarrierThread");
  }
  jvmtiError err = (*GetCarrierThread_func)(jvmti, vthread, cthread_ptr);

  return err;
}

static jthread
get_virtual_thread(jvmtiEnv* jvmti, JNIEnv* jni, jthread cthread) {
  jthread vthread = NULL;
  jvmtiError err = GetVirtualThread(jvmti, jni, cthread, &vthread);
  check_jvmti_status(jni, err, "jvmti_common get_virtual_thread: Error in JVMTI extension GetVirtualThread");
  return vthread;
}

static jthread
get_carrier_thread(jvmtiEnv* jvmti, JNIEnv* jni, jthread vthread) {
  jthread cthread = NULL;
  jvmtiError err = GetCarrierThread(jvmti, jni, vthread, &cthread);
  check_jvmti_status(jni, err, "jvmti_common get_carrier_thread: Error in JVMTI extension GetCarrierThread");

  return cthread;
}

static jvmtiExtensionEventInfo*
find_ext_event(jvmtiEnv* jvmti, const char* ename) {
  jint extCount = 0;
  jvmtiExtensionEventInfo* extList = NULL;

  jvmtiError err = jvmti->GetExtensionEvents(&extCount, &extList);
  if (err != JVMTI_ERROR_NONE) {
    printf("jvmti_common find_ext_event: Error in JVMTI GetExtensionFunctions: %s(%d)\n",
           TranslateError(err), err);
    printf(0);
    return NULL;
  }
  for (int i = 0; i < extCount; i++) {
    if (strstr(extList[i].id, (char*)ename) != NULL) {
      return &extList[i];
    }
  }
  return NULL;
}

static jvmtiError
set_ext_event_callback(jvmtiEnv* jvmti,  const char* ename, jvmtiExtensionEvent callback) {
  jvmtiExtensionEventInfo* info = find_ext_event(jvmti, ename);

  if (info == NULL) {
    printf("jvmti_common set_ext_event_callback: Extension event was not found: %s\n", ename);
    return JVMTI_ERROR_NOT_AVAILABLE;
  }
  jvmtiError err = jvmti->SetExtensionEventCallback(info->extension_event_index, callback);
  return err;
}

/** Enable or disable given events. */

static jvmtiError
set_event_notification_mode(jvmtiEnv* jvmti, jvmtiEventMode mode, jvmtiEvent event_type, jthread event_thread) {
  jvmtiError err = jvmti->SetEventNotificationMode(mode, event_type, event_thread);
  return err;
}

static void
set_event_notification_mode(jvmtiEnv* jvmti, JNIEnv* jni, jvmtiEventMode mode, jvmtiEvent event_type, jthread event_thread) {
  jvmtiError err = jvmti->SetEventNotificationMode(mode, event_type, event_thread);
  check_jvmti_status(jni, err, "jvmti_common set_event_notification_mode: Error in JVMTI SetEventNotificationMode");
}

int
nsk_jvmti_enableEvents(jvmtiEnv* jvmti, JNIEnv* jni, jvmtiEventMode enable, int size, jvmtiEvent list[], jthread thread) {
  for (int i = 0; i < size; i++) {
    check_jvmti_status(jni, jvmti->SetEventNotificationMode(enable, list[i], thread), "");
  }
  return NSK_TRUE;
}

void
millisleep(int millis) {
#ifdef _WIN32
  Sleep(millis);
#else
  usleep(1000 * millis);
#endif
}

void
nsk_jvmti_sleep(jlong timeout) {
  int seconds = (int)((timeout + 999) / 1000);
#ifdef _WIN32
  Sleep(1000L * seconds);
#else
  sleep(seconds);
#endif
}

#endif
