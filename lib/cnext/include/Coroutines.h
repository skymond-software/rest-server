///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.12.2022
///
/// @file              Coroutines.h
///
/// @brief             Minimal coroutines for C.
///
/// @details           Originally written by Tony Finch <dot@dotat.at> and
///                    released to the public domain (see
///                    http://creativecommons.org/publicdomain/zero/1.0/).
///                    The API was modeled after Lua's coroutines (see
///                    http://www.lua.org/manual/5.1/manual.html#2.11).
///                    Source code cloned from
///                    git://git.chiark.greenend.org.uk/~fanf/picoro.git
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef COROUTINES_H
#define COROUTINES_H

#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

#include "CoroutineSync.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Forward declarations.  Doxygen below.
typedef struct Coroutine Coroutine;
typedef struct msg_t msg_t;

#include "Messages.h"

// Base coroutine support.

// Coroutine status values.
#define coroutineSuccess  0
#define coroutineBusy     1
#define coroutineError    2
#define coroutineNomem    3
#define coroutineTimedout 4

/// @def COROUTINE_NOT_RESUMABLE
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine cannot be resumed, either because it is blocked or
/// because it has completed.
#define COROUTINE_NOT_RESUMABLE ((void*) ((intptr_t) -1))

/// @def COROUTINE_TIMEDWAIT
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine is waiting on a condition or mutex with a timeout.
#define COROUTINE_TIMEDWAIT ((void*) ((intptr_t) -2))

/// @def COROUTINE_WAIT
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine is waiting on a condition or mutex.
#define COROUTINE_WAIT ((void*) ((intptr_t) -3))

/// @def COROUTINE_CORRUPT
///
/// @brief Special value to indicate to a caller of coroutineResume() that the
/// provided coroutine's state has been corrupted and is not longer usable.
#define COROUTINE_CORRUPT ((void*) ((intptr_t) -4))

/// @def COROUTINE_GUARD_VALUE
///
/// @brief The value to set for a Coroutine's guard1 and guard2 elements and to
/// check against in coroutineResume before a resume of a coroutine is
/// attempted.
#define COROUTINE_GUARD_VALUE 0x4abc4abc

/// @def COROUTINE_STACK_CHUNK_SIZE
///
/// @brief The size of a single chunk of the stack allocated by
/// coroutineAllocateStack.
#define COROUTINE_STACK_CHUNK_SIZE 32

/// @def COROUTINE_DEFAULT_STACK_SIZE
///
/// @brief The default stack size to allocate, in bytes.
#ifndef COROUTINE_DEFAULT_STACK_SIZE
#define COROUTINE_DEFAULT_STACK_SIZE 16384
#endif

/// @def CoroutineId
///
/// @brief The integer type to use for coroutine IDs.  Defaults to 64-bit IDs
/// if none is provided.
#ifndef CoroutineId
#define CoroutineId uint64_t
#endif

/// @def COROUTINE_ID_TYPE_int64_t_int64_t
///
/// @brief Define that gets matched when CoroutineId is an uint64_t.
#define COROUTINE_ID_TYPE_uint64_t_uint64_t 1

/// @def COROUTINE_ID_TYPE_int32_t_int32_t
///
/// @brief Define that gets matched when CoroutineId is an uint32_t.
#define COROUTINE_ID_TYPE_uint32_t_uint32_t 1

/// @def COROUTINE_ID_TYPE_int16_t_int16_t
///
/// @brief Define that gets matched when CoroutineId is an uint16_t.
#define COROUTINE_ID_TYPE_uint16_t_uint16_t 1

/// @def COROUTINE_ID_TYPE_int8_t_int8_t
///
/// @brief Define that gets matched when CoroutineId is an uint8_t.
#define COROUTINE_ID_TYPE_uint8_t_uint8_t   1

/// @def EXPAND_COROUTINE_ID_TYPE
///
/// @brief Second-level macro to fully expand the types passed into
/// TEST_COROUTINE_ID_TYPE.
#define EXPAND_COROUTINE_ID_TYPE(type1, type2) \
  COROUTINE_ID_TYPE_##type1##_##type2

/// @def TEST_COROUTINE_ID_TYPE
///
/// @brief Determine if the type specified by type1 matches the type specified
/// by type2.
#define TEST_COROUTINE_ID_TYPE(type1, type2) \
  EXPAND_COROUTINE_ID_TYPE(type1, type2)

/// @def COROUTINE_ID_NOT_SET
///
/// @brief Special value to indicate that a coroutine's ID value is not set.
/// This is the initial value just after the coroutine constructor completes.
#ifndef COROUTINE_ID_NOT_SET
#if TEST_COROUTINE_ID_TYPE(CoroutineId, uint64_t)
#define COROUTINE_ID_NOT_SET ((uint64_t) 0xffffffffffffffff)
#elif TEST_COROUTINE_ID_TYPE(CoroutineId, uint32_t)
#define COROUTINE_ID_NOT_SET ((uint32_t) 0xffffffff)
#elif TEST_COROUTINE_ID_TYPE(CoroutineId, uint16_t)
#define COROUTINE_ID_NOT_SET ((uint16_t) 0xffff)
#elif TEST_COROUTINE_ID_TYPE(CoroutineId, uint8_t)
#define COROUTINE_ID_NOT_SET ((uint8_t) 0xff)
#else
#error "Invalid type for CoroutineId."
#endif // CoroutineId
#endif // COROUTINE_ID_NOT_SET

/// @enum CoroutineState
///
/// @brief States that a Coroutine can be in.
typedef enum CoroutineState {
  COROUTINE_STATE_NOT_RUNNING,
  COROUTINE_STATE_RUNNING,
  COROUTINE_STATE_BLOCKED,
  NUM_COROUTINE_STATES
} CoroutineState;

/// @typedef CoroutineFunction
///
/// @brief Function signature that can be used as a coroutine.
typedef void* (*CoroutineFunction)(void *arg);

/// @typedef ComutexUnlockCallback
///
/// @brief Function signature that can be used as a callback when a comutex is
/// unlocked.
typedef void (*ComutexUnlockCallback)(void *stateData, Comutex *comutex);

/// @typedef CoconditionSignalCallback
///
/// @brief Function signature that can be used as a callback when a cocondition
/// is signalled.
typedef void (*CoconditionSignalCallback)(void *stateData,
  Cocondition *cocondition);

/// @union CoroutineFuncData
///
/// @brief Translation between a function pointer and a data pointer.
///
/// @details Due to the way this library works, we sometimes need to pass and
/// return function pointers to our yield and resume functions, which take and
/// return data pointers.  ISO C doesn't permit casting between these two, so
/// we use a union to do the conversion.
///
/// @param func The function pointer portion of the pointer value.
/// @param data The data pointer portion of the pointer value.
typedef union CoroutineFuncData {
  CoroutineFunction func;
  void *data;
} CoroutineFuncData;

// Coroutine base support.

/// @struct Coroutine
///
/// @brief Data structure to manage an individual coroutine.
///
/// @param guard1 A well-known value to check for state corruption (stack
///   overflow).
/// @param nextInList Pointer to the next Coroutine in the list.
/// @param context The jmp_buf to hold the context of the coroutine.
/// @param id The ID of the coroutine.
/// @param state The state of the coroutine.  (See enum above.)
/// @param nextToLock The next coroutine to allow to lock a mutex.
/// @param prevToLock The previous coroutine to allow to lock a mutex.
/// @param nextToSignal The next coroutine to signal when waiting on a signal.
/// @param prevToSignal The previous coroutine to signal when waiting on a
///   signal.
/// @param resetContext The jmp_buf that holds the place on stack to jump to
///   after a coroutine has been terminated and the value of context is reset.
/// @param passed The CoroutineFuncData that's passed between contexts by the
///   coroutinePass function (on a yield or resume call).
/// @param messageQueue A msg_q_t that holds the messages sent to this
///   coroutine.
/// @param blockingComutex A pointer to the mutex (Comutex) that the coroutine
///   is currently waiting to lock.
/// @param blockingCocondition A pointer to a condition (Cocondition) that the
///   coroutine is currently waiting on to be signalled.
/// @param guard2 A well-known value to check for state corruption (stack
///   overflow).
typedef struct Coroutine {
  uint32_t guard1;
  struct Coroutine *nextInList;
  jmp_buf context;
  CoroutineId id;
  CoroutineState state;
  struct Coroutine *nextToLock;
  struct Coroutine *prevToLock;
  struct Coroutine *nextToSignal;
  struct Coroutine *prevToSignal;
  jmp_buf resetContext;
  CoroutineFuncData passed;
  msg_q_t messageQueue;
  Comutex *blockingComutex;
  Cocondition *blockingCocondition;
  uint32_t guard2;
} Coroutine;

// Coroutine message support.

// Support functions
int64_t coroutineGetNanoseconds(const struct timespec *ts);

/// @def coroutineResumable(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it can be resumed.
/// A coroutine can be resumed if it is not on the running or idle lists.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns false when the coroutine has run to completion or when it is
/// blocked inside coroutineResume() and true otherwise.
#define coroutineResumable(coroutinePointer) \
  (((coroutinePointer) != NULL) && ((coroutinePointer)->nextInList == NULL))

/// @def coroutineFinished(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it has completed.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns true when the coroutine is allocated and its state
/// indicates that it's no longer running.
#define coroutineFinished(coroutinePointer) \
  (((coroutinePointer) != NULL) \
    && ((coroutinePointer)->state == COROUTINE_STATE_NOT_RUNNING))

/// @def coroutineRunning(coroutinePointer)
///
/// @brief Examines a coroutine to determine whether or not it is still running.
///
/// @param coroutinePointer A pointer to the Coroutine to examine.
///
/// @return Returns true when the coroutine is allocated and its state
/// indicates that it is still running.
#define coroutineRunning(coroutinePointer) \
  (((coroutinePointer) != NULL) \
    && ((coroutinePointer)->state != COROUTINE_STATE_NOT_RUNNING))

/// @def getRunningCoroutineId
///
/// @brief Get the coroutine ID for the currently-running coroutine.
#define getRunningCoroutineId() \
  coroutineId(getRunningCoroutine())

// Coroutine function prototypes.  Doxygen inline in source file.
int coroutineConfig(Coroutine *first, int stackSize, void *stateData,
  ComutexUnlockCallback comutexUnlockCallback,
  CoconditionSignalCallback coconditionSignalCallback
);
Coroutine* coroutineInit(Coroutine *userCoroutine,
  CoroutineFunction func, void *arg);
int coroutineCreate(Coroutine **coroutine, CoroutineFunction func, void *arg);
void* coroutineResume(Coroutine *targetCoroutine, void *arg);
void* coroutineYield(void *arg);
int coroutineSetId(Coroutine *coroutine, CoroutineId id);
CoroutineId coroutineId(Coroutine *coroutine);
CoroutineState coroutineState(Coroutine *coroutine);
#ifdef THREAD_SAFE_COROUTINES
void coroutineSetThreadingSupportEnabled(bool state);
bool coroutineThreadingSupportEnabled();
#endif
int coroutineTerminate(Coroutine *targetCoroutine, Comutex **mutexes);
Coroutine* getRunningCoroutine(void);


// Message queue functions
msg_t* comessageQueuePeek(void);
msg_t* comessageQueuePop(void);
msg_t* comessageQueuePopType(int type);
msg_t* comessageQueueWait(const struct timespec *ts);
msg_t* comessageQueueWaitForType(int64_t type, const struct timespec *ts);
int comessageQueuePush(Coroutine *coroutine, msg_t *comessage);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // COROUTINES_H
