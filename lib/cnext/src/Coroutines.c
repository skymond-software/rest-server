////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
 * Some notes about the way(s) this library works.
 *
 * Tony Finch is an extremely clever programmer and I am forever indebted to
 * him for his initial work on this.  His approach to this is, in my opinion,
 * sheer genius.  It is, however, not straightforward and requires some
 * explanation.
 *
 * This approach makes use of two stacks:  A stack of running coroutines and a
 * stack of idle coroutines.  Prior to coroutineConfig being called, both stacks
 * are empty.  coroutineConfig initializes the running stack to the Coroutine
 * passed in and sets the stack size that is to be used, but leaves the idle
 * stack alone.  The idle stack is not modified until the first call to
 * coroutineCreate is made.  From that point on, there is always at least one
 * Coroutine on the idle stack.
 *
 * When coroutineCreate is called, it first checks the idle stack to see if
 * there is anything available.  The only time there won't be anything available
 * is on the first time this function is called (per thread).  If nothing is
 * available, coroutineAllocateStack is called with the stack size that was
 * provided to coroutineConfig.  This will allocate the remainder of the stack
 * for the main function, allocate a Coroutine structure, push a pointer to it
 * onto the idle stack, and then resume execution in coroutineCreate.  If a
 * Coroutine is available when coroutineCreate does its check, it simply pops
 * the stack.
 *
 * Once coroutineCreate has an idle Coroutine, it pushes the Coroutine onto the
 * running stack and then passes it the function pointer it was provided.  This
 * resumes execution in the coroutineMain function.  There are two possible
 * places in coroutineMain where execution may resume.  If the Coroutine that
 * was pulled from the idle stack had been used previously and completed, then
 * execution resumes at the end of the main while loop where the function is
 * picked up as the value returned from its yield call.  However, if this is the
 * Coroutine that was last created, then its point of entry will be near the
 * beginning of coroutineMain.
 *
 * In the former case, coroutineMain just yields back to the calling Coroutine
 * and waits for its arguments to be provided by coroutineResume.  In the latter
 * case, things aren't so simple.  In this case, the provided function pointer
 * is stored and then coroutineAllocateStack is called again to allocate another
 * Coroutine.  As before, this coroutine is pushed onto the idle stack, however
 * when it does a longjmp this time, execution is not resumed in the
 * coroutineCreate constructor but in the previous-level coroutineMain call.
 * This then enters the main while loop which yields control back to the calling
 * coroutine and waits for its arguments to be provided (just as in the former
 * case above).
 *
 * From this point onward, passing control between coroutines is done by a
 * running coroutine calling coroutineYield to yield control back to its caller
 * and a calling function calling coroutineResume to resume execution in the
 * called coroutine from the point at which the coroutine made its last
 * coroutineYield call (or from the beginning of the function if the funcion has
 * not yet begun execution).
 *
 * coroutineCreate, coroutineMain, coroutineResume, and coroutineYield make use
 * of an internal function called coroutinePass and/or raw setjmp and longjmp
 * function calls.  coroutinePass also makes use of setjmp and longjmp.
 * coroutinePass takes as parameters a pointer to the Coroutine making the call
 * and the value that is to be passed to the Coroutine whose execution is
 * resumed.  Execution will be passed to the Coroutine that is at the top of the
 * running stack.  It is the responsibility of the caller to ensure that the
 * running stack is setup correctly before calling coroutinePass.
 *
 * The second parameter of coroutinePass is a CoroutineFuncData union.  This is
 * a union of a data pointer and a function pointer.  The reason for this union
 * is that both function pointers and data pointers have to be passed through
 * coroutinePass, however the C standard forbids casting function pointers to
 * data pointers and vice-versa.  So, we need a union in order to be compliant
 * with the specification.  It is the responsibility of the caller to know which
 * kind of value to provide and which kind of value to expect.  Note that
 * coroutineResume and coroutineYield only deal with data pointers, so the
 * function pointer use case can only be exercised by functions within this
 * library (because coroutinePass is not an externally-visible function).  This
 * is deliberate and this is what is desirable.
 *
 * Some implications and limitations...
 *
 * The fact that there is always at least one Coroutine on the idle stack has
 * some implications for memory-constrained environments.  The first time that
 * coroutineCreate is called, the upper limit for the calling function's stack
 * -AND- the stack for the created Coroutine are both allocated.  Allocation
 * is achieved by recursively calling a function with a character buffer, which
 * means that the memory in those stacks will be touched during this process.
 * Effectively, the first time that coroutineCreate is called, a little over 2X
 * the stack size provided to coroutineConfig is touched.  This can result in
 * a nasty surprise (i.e. a crash) in severly memory-constrained environments.
 *
 * One requirement of this system is that all the stacks must be the same size.
 * The stack size provided to coroutineConfig cannot be changed once the first
 * Coroutine has been created with coroutineCreate.  This is because the size
 * of a stack for a Coroutine is actually provided to coroutineAllocateStack on
 * the call prior to the call that returns a usable Coroutine.  It would be very
 * difficult (and highly confusing) to try and adjust the size dynamically.  To
 * save us all any more confusion than this system already causes, all stacks on
 * a single thread must be the same size.
 *
 * JBC 2024-11-18
 */

// Doxygen marker
/// @file

#include "Coroutines.h"

#ifdef THREAD_SAFE_COROUTINES
#include "CThreads.h"
#endif

#include <string.h>
#include <stdio.h> // For error messages

// Prototype forward declarations for mutual recursion.
void coroutineAllocateStack(int stackSize, void *topOfStack);
void coroutineMain(void *stack);

/// @def ZEROINIT
///
/// @brief Define the proper way to declare a zeroized variable based on the
/// compiler environment.
#ifdef __cplusplus
#ifdef _MSC_VER
#define ZEROINIT(x) x = {0}
#else // Non-Visual Studio C++
#define ZEROINIT(x) x = {}
#endif // _MSC_VER
#else // __cplusplus not defined
#define ZEROINIT(x) x = {0}
#endif // __cplusplus

// Use statically-allocated global variables for use wihtout threading.

/// @var static Coroutine *_globalFirst
///
/// @brief Library-private first (main) routine.
static Coroutine *_globalFirst = NULL;

/// @var static Coroutine *_globalRunning
///
/// @brief Library-private head of the running list.
///
/// @details The coroutine at the head of the running LIFO list has the CPU, and
/// all others are suspended inside coroutineResume(). The first Coroutine
/// object holds the context for the process's initial stack and also ensures
/// that all externally-visible list elements have non-NULL next pointers.
static Coroutine* _globalRunning = NULL;

/// @var static Coroutine *_globalIdle
///
/// @brief Library-private head of the idle list.
///
/// @details The idle LIFO list contains coroutines that are suspended in
/// coroutineMain() and available to be associated with a new function. After
/// initialization it is never NULL except briefly while coroutineMain() forks
/// a new idle coroutine.
static Coroutine* _globalIdle = NULL;

/// @var static int _globalStackSize
///
/// @brief The size of each coroutine's stack in bytes.
static int _globalStackSize = COROUTINE_DEFAULT_STACK_SIZE;

/// @fn void coroutineGlobalPush(Coroutine **list, Coroutine *coroutine)
///
/// @brief Add a coroutine to a global list and get the previous head of the
/// list.
///
/// @param list The Coroutine list to push onto.
/// @param coroutine A pointer to the Coroutine to push onto the list.
///
/// @param Returns the previous head of the list.
void coroutineGlobalPush(Coroutine** list, Coroutine* coroutine) {
  if ((list != NULL) && (coroutine != NULL)) {
    coroutine->next = *list;
    *list = coroutine;
  }
}

/// @fn Coroutine* coroutineGlobalPop(Coroutine **list)
///
/// @brief Remove a coroutine from a global list and return it.
///
/// @param list The Coroutine list to pop from.
///
/// @return Returns a pointer to the previous head of the list.
Coroutine* coroutineGlobalPop(Coroutine** list) {
  Coroutine* coroutine = NULL;

  if (list != NULL) {
    coroutine = *list;
    *list = coroutine->next;
    coroutine->next = NULL;
  }

  return coroutine;
}

#ifdef THREAD_SAFE_COROUTINES
// Use dynamic memory and C threads mechanisms.

/// @var static bool _coroutineThreadingSupportEnabled
///
/// @brief State variable to determine whether or not threading support is
/// enabled in systems where threading is possible.  Defaults to false.
static bool _coroutineThreadingSupportEnabled = false;

/// @var static tss_t _tssFirst
///
/// @brief Thread-specific first (main) routine.
ZEROINIT(static tss_t _tssFirst);

/// @var static tss_t _tssRunning
///
/// @brief Thread-specific head of the running list.
///
/// @details The coroutine at the head of the running LIFO list has the CPU, and
/// all others are suspended inside coroutineResume(). The "first" coro object
/// holds the context for the process's initial stack and also ensures that all
/// externally-visible list elements have non-NULL next pointers.  (The "first"
/// coroutine isn't exposed to the caller.)
ZEROINIT(static tss_t _tssRunning);

/// @var static tss_t _tssIdle
///
/// @brief Thread-specific head of the idle list.
///
/// @details The idle LIFO list contains coroutines that are suspended in
/// coroutineMain() and available to be associated with a new function. After
/// initialization it is never NULL except briefly while coroutineMain() forks
/// a new idle coroutine.
ZEROINIT(static tss_t _tssIdle);

/// @var static tss_t _tssStackSize
///
/// @brief Thread-specific size of each coroutine's stack, in bytes.
ZEROINIT(static tss_t _tssStackSize);

/// @var static once_flag _threadMetadataSetup
///
/// @brief once_flag to make sure we only initialize the thread-specific storage
/// once.
static once_flag _threadMetadataSetup = ONCE_FLAG_INIT;

/// @fn void coroutineSetupThreadMetadata(void)
///
/// @brief Setup the global thread-specefic storage needed for this library.
///
/// @return This function returns no value.
void coroutineSetupThreadMetadata(void) {
  // _first is the only Coroutine node that will be allocated with dynamic
  // memory, so it's the only one that needs a destructor.  All the other
  // nodes on the _running and _idle lists will be on the stack and do not
  // need destructors.
  int status = tss_create(&_tssFirst, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _tssFirst.\n");
  }
  status = tss_create(&_tssRunning, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _tssRunning.\n");
  }
  status = tss_create(&_tssIdle, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _tssIdle.\n");
  }
  status = tss_create(&_tssStackSize, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _tssStackSize.\n");
  }
}

/// @fn void coroutineInitializeThreadMetadata(Coroutine *first)
///
/// @brief Initialize the thread-specific storage on a per-thread basis.
///
/// @param first The coroutine to use as the first coroutine on the current
///   thread.  This value may not be NULL on the first call to this function
///   on the thread.
///
/// @return Returns true on success, false on failure.
bool coroutineInitializeThreadMetadata(Coroutine *first) {
  if (tss_get(_tssFirst) != NULL) {
    // This thread is already set up.  Nothing to do.
    return true;
  }

  if (first == NULL) {
    fprintf(stderr, "ERROR:  NULL first Coroutine provided to "
      "coroutineInitializeThreadMetadata.\n");
    return false;
  }

  int status = tss_set(_tssFirst, first);
  if (status != thrd_success) {
    fprintf(stderr,
      "Could not set _tssFirst to first in "
      "coroutineInitializeThreadMetadata.\n");
    return false;
  }
  status = tss_set(_tssRunning, first);
  if (status != thrd_success) {
    fprintf(stderr,
      "Could not set _tssRunning to first in "
      "coroutineInitializeThreadMetadata.\n");
    return false;
  }
  // This *shouldn't* be necessary, but why assume when we can be certain?
  status = tss_set(_tssIdle, NULL);
  if (status != thrd_success) {
    fprintf(stderr,
      "Could not set _tssIdle to NULL in coroutineInitializeThreadMetadata.\n");
    return false;
  }
  status = tss_set(_tssStackSize, (void*) ((intptr_t) _globalStackSize));
  if (status != thrd_success) {
    fprintf(stderr,
      "Could not set _tssStackSize to %d in "
      "coroutineInitializeThreadMetadata.\n", _globalStackSize);
    return false;
  }

  return true;
}

/// @fn void coroutineTssPush(tss_t* list, Coroutine *coroutine)
///
/// @brief Add a coroutine to a thread-specific storage list and get the
/// previous head of the list.
///
/// @param list The Coroutine list to push onto.
/// @param coroutine A pointer to the Coroutine to push onto the list.
///
/// @param Returns the previous head of the list.
void coroutineTssPush(tss_t* list, Coroutine* coroutine) {
  if ((list != NULL) && (coroutine != NULL)) {
    coroutine->next = (Coroutine*) tss_get(*list);
    tss_set(*list, coroutine);
  }
}

/// @fn Coroutine* coroutineTssPop(tss_t* list)
///
/// @brief Remove a coroutine from a thread-specific storage list and return it.
///
/// @param list The Coroutine list to pop from.
///
/// @return Returns a pointer to the previous head of the list.
Coroutine* coroutineTssPop(tss_t* list) {
  Coroutine* coroutine = NULL;

  if (list != NULL) {
    coroutine = (Coroutine*) tss_get(*list);
    tss_set(*list, coroutine->next);
    coroutine->next = NULL;
  }

  return coroutine;
}

#endif // THREAD_SAFE_COROUTINES

/// @fn void* coroutinePass(Coroutine currentCoroutine, CoroutineFuncData arg)
///
/// @brief Pass a value and control from one coroutine to another.  The target
/// coroutine is at the head of the "running" list.
///
/// @param currentCoroutine A pointer to the current coroutine's state.
/// @param arg The value to pass to the target coroutine.
///
/// @return Returns the target's returned or yielded value.
CoroutineFuncData coroutinePass(Coroutine *currentCoroutine, CoroutineFuncData arg) {
  ZEROINIT(CoroutineFuncData returnValue);
  
  if (currentCoroutine != NULL) {
    if (!setjmp(currentCoroutine->context)) {
#ifdef THREAD_SAFE_COROUTINES
      Coroutine* targetCoroutine = NULL;
      if (!_coroutineThreadingSupportEnabled) {
        targetCoroutine = _globalRunning;
      } else {
        targetCoroutine = (Coroutine*) tss_get(_tssRunning);
      }
#else
      Coroutine *targetCoroutine = _globalRunning;
#endif

      if (targetCoroutine != NULL) {
#if defined(_MSC_VER) && defined(_M_X64)
        // This should *NOT* be necessary.  The intent of longjmp is to restore
        // the context of the registers captured at setjmp, however the MSVC x64
        // implementation of longjmp only does this if the value of
        // _JUMP_BUFFER.Frame is 0.  This is a non-standard and broken
        // implementation, but thankfully a workaround does exist, so I won't
        // complain beyond this comment.
        _JUMP_BUFFER* context = (_JUMP_BUFFER*) &targetCoroutine->context;
        context->Frame = 0;
#endif // _MSC_VER
        targetCoroutine->passed = arg;
        longjmp(targetCoroutine->context, 1);
      }
    }
  } else {
    return returnValue; // NULL return value.
  }

  returnValue = currentCoroutine->passed;
  return returnValue;
}

/// @fn void* coroutineResume(Coroutine *targetCoroutine, void *arg)
///
/// @brief Transfer control to another coroutine.  A coroutine that is blocked
/// inside coroutineResume() is not resumable.
///
/// @param targetCoroutine A pointer to the Coroutine to resume.
/// @param arg Returned by coroutineYield() inside the target coroutine or
///   passed as the coroutine's parameter on the first call to coroutineResume
///   for the coroutine.
///
/// @return If the coroutine is resuamable, returns the value provided to the
///   yield call from within the coroutine or the coroutine's return value if it
///   has run to completion.  If the coroutine is not resumable, returns the
///   special value COROUTINE_NOT_RESUMABLE.
void* coroutineResume(Coroutine *targetCoroutine, void *arg) {
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return NULL;
    }
  }
#endif

  if (coroutineResumable(targetCoroutine)) {
    Coroutine *currentCoroutine = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
    if (_coroutineThreadingSupportEnabled) {
      currentCoroutine = (Coroutine*) tss_get(_tssRunning);
    }
#endif

    if (currentCoroutine == NULL) {
      // The running stack hasn't been defined yet.  Bail.
      return NULL;
    }

#ifdef THREAD_SAFE_COROUTINES
    if (!_coroutineThreadingSupportEnabled) {
      coroutineGlobalPush(&_globalRunning, targetCoroutine);
    } else {
      coroutineTssPush(&_tssRunning, targetCoroutine);
    }
#else
    coroutineGlobalPush(&_globalRunning, targetCoroutine);
#endif
    // The target coroutine is now at the head of the running list as is
    // expected by coroutinePass().
    CoroutineFuncData funcData;
    funcData.data = arg;
    funcData = coroutinePass(currentCoroutine, funcData);
    return funcData.data;
  }

  return COROUTINE_NOT_RESUMABLE;
}

/// @fn void* coroutineYield(void *arg)
///
/// @brief Transfer control back to the coroutine that resumed this one.  A
/// coroutine that is blocked inside coroutineYield() may be resumed by any
/// other coroutine.
///
/// @param arg Value that will be returned by coroutineResume().
///
/// @return Returns the value passed into the next call to coroutineResume()
/// for this coroutine.
void* coroutineYield(void *arg) {
  void *returnValue = NULL;
  Coroutine* first = _globalFirst;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return NULL;
    }
    first = (Coroutine*) tss_get(_tssFirst);
  }
#endif

  Coroutine *running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif
  if (running == first) {
    // Can't yield from the main coroutine.  Return NULL.
    return NULL;
  } else if (running == NULL) {
    // The running stack hasn't been setup yet.  Bail.
    return NULL;
  }

  Coroutine *currentCoroutine = NULL;
#ifdef THREAD_SAFE_COROUTINES
  if (!_coroutineThreadingSupportEnabled) {
    currentCoroutine = coroutineGlobalPop(&_globalRunning);
  } else {
    currentCoroutine = coroutineTssPop(&_tssRunning);
  }
#else
  currentCoroutine = coroutineGlobalPop(&_globalRunning);
#endif

  currentCoroutine->state = COROUTINE_STATE_BLOCKED;
  CoroutineFuncData funcData;
  funcData.data = arg;
  funcData = coroutinePass(currentCoroutine, funcData);
  currentCoroutine->state = COROUTINE_STATE_RUNNING;
  returnValue = funcData.data;

  return returnValue;
}

/// @fn Coroutine* coroutineCreate(void* func(void *arg))
///
/// @brief The coroutine constructor function.
///
/// @details Create a coroutine that will run func(). The coroutine starts off
/// suspended.  When it is first resumed, the argument to coroutineResume() is
/// passed to func().  If func() returns, its return value is returned by
/// coroutineResume() as if the coroutine yielded, except that the coroutine is
/// then no longer resumable and may be discarded (*NOT* freed since its
/// allocation is on the stack and not the heap).
///
/// On the first invocation there are no idle coroutines, so fork the first one,
/// which will immediately yield back to us after becoming idle. When there are
/// idle coroutines, we pass one the function pointer and return the activated
/// coroutine's address.
///
/// @param func The coroutine function to run.
///
/// @return Returns a newly-initialized Coroutine on success, NULL on failure.
Coroutine* coroutineCreate(void* func(void *arg)) {
  CoroutineFuncData funcData;
  funcData.func = func;
  if (funcData.data == NULL) {
    // Can't pass a NULL function pointer (this would crash).  Return a NULL
    // Coroutine.  This will cause any future calls with this returned value to
    // coroutineResumable to return false and calls to coroutineResume to return
    // COROUTINE_NOT_RESUMABLE.
    return NULL;
  }

  Coroutine* idle = _globalIdle;
  Coroutine* running = _globalRunning;
  int stackSize = _globalStackSize;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return NULL;
    }
    stackSize = (int) ((intptr_t) tss_get(_tssStackSize));
    idle = (Coroutine*) tss_get(_tssIdle);
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif

  if (running == NULL) {
    fprintf(stderr, "ERROR: No first coroutine defined.\n");
    fprintf(stderr, "       Cannot create coroutine.\n");
    return NULL;
  }

  // The current coroutine is at the head of the running list.
  if ((idle == NULL) && (!setjmp(running->context))) {
    // We've just been called from the calling function and need to create a
    // new Coroutine instance, including its stack.
    coroutineAllocateStack(stackSize, NULL);
  }
  // Either there was an idle coroutine on the idle list or we just returned
  // from coroutineMain (called by coroutineAllocateStack).  Either way, the Coroutine
  // instance we want to use is now at the head of the idle list.

  // The head of the idle list has the Coroutine allocated in coroutineMain.
#ifdef THREAD_SAFE_COROUTINES
  Coroutine* newCoroutine = NULL;
  if (!_coroutineThreadingSupportEnabled) {
    newCoroutine = coroutineGlobalPop(&_globalIdle);
  } else {
    newCoroutine = coroutineTssPop(&_tssIdle);
  }
#else
  Coroutine *newCoroutine = coroutineGlobalPop(&_globalIdle);
#endif

  // The head of the running list is the current coroutine.
  // We need to run coroutineResume() and pass in the function pointer we were
  // provided as the argument to the coroutine, however coroutineResume() takes
  // a void* as its argument, not a function pointer.  coroutinePass() takes
  // the union of a data pointer and a funciton pointer, so it's legal to pass
  // a function pointer to that.  So, the logic below is the logic of
  // coroutineResume() with the appropriate variable substitution done so that
  // we can legally pass a funciton pointer and retrieve the Coroutine pointer.
  // We don't need to do the thread setup logic at the start of the function
  // since that's done above and we know that newCoroutine is resuamble, so we
  // can skip that check.
  Coroutine *currentCoroutine = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (!_coroutineThreadingSupportEnabled) {
    coroutineGlobalPush(&_globalRunning, newCoroutine);
  } else {
    currentCoroutine = (Coroutine*) tss_get(_tssRunning);
    coroutineTssPush(&_tssRunning, newCoroutine);
  }
#else
  coroutineGlobalPush(&_globalRunning, newCoroutine);
#endif
  // The target coroutine is now at the head of the running list as is
  // expected by coroutinePass().  funcData contains the coroutine that was
  // passed to this function.
  funcData = coroutinePass(currentCoroutine, funcData);
  newCoroutine = (Coroutine*) funcData.data;

  return newCoroutine;
}

/// void coroutineMain(void *stack)
///
/// @brief The main loop responsible for managing the "idle" list.
///
/// @details When we start the idle list is empty, so we put ourself on it to
/// ensure it remains non-NULL. Then we immediately suspend ourself waiting for
/// the first function we are to run. (The head of the running list is the
/// coroutine that forked us.) We pass the stack pointer to prevent it from
/// being optimised away. The first time we are called we will return to the
/// fork in the coroutineCreate() constructor function (above); on subsequent
/// calls we will resume the parent coroutineMain(). In both cases the passed
/// value is lost when coroutinePass() longjmp()s to the forking setjmp().
///
/// When we are resumed, the idle list is empty again, so we fork another
/// coroutine. When the child coroutineMain() passes control back to us, we drop
/// into our main loop.
///
/// We are now head of the running list with a function to call. We immediately
/// yield a pointer to our context object so our creator can identify us. The
/// creator can then resume us at which point we pass the argument to the
/// function to start executing.
///
/// When the function returns, we move ourself from the running list to the idle
/// list, before passing the result back to the resumer. (This is just like
/// coroutineYield() except for adding the coroutine to the idle list.) We can
/// then only be resumed by the coroutineCreate() constructor function which
/// will put us back on the running list and pass us a new function to call.
///
/// @param stack A pointer to the stack that was allocated for the coroutine.
///   This parameter is taken from the stack allocation function so that the
///   stack allocated there doesn't get optimized out by the compiler.
///
/// @return This function returns no value and, in fact, never returns.
void coroutineMain(void *stack) {
  ZEROINIT(Coroutine me);
  me.id = COROUTINE_ID_NOT_SET;
#ifdef THREAD_SAFE_COROUTINES
  if (!_coroutineThreadingSupportEnabled) {
    coroutineGlobalPush(&_globalIdle, &me);
  } else {
    coroutineTssPush(&_tssIdle, &me);
  }
#else
  coroutineGlobalPush(&_globalIdle, &me);
#endif

  // The target of coroutinePass() (the caller) is at the head of the running
  // list.  The return point for that Coroutine was either set in the setjmp
  // call in the coroutine constructor or in the setjmp call below.  In the
  // former case, calling coroutinePass() here returns to the constructor and
  // waits for the constructor to provide the function pointer to call.  In the
  // latter case, we allocate the next Coroutine and its stack that will be
  // pushed onto the idle list (above).  When we call pass here, we will release
  // the Coroutine currently being constructed from the setjmp below, allowing
  // it to drop into its main loop and we will be on the idle stack ready to
  // take in a new function pointer when we're resumed.
  CoroutineFuncData funcData;
  funcData.data = stack;
  funcData = coroutinePass(&me, funcData);
  void *(*func)(void *arg);
  func = funcData.func;

  // At this point, we've been passed execution from the constructor calling
  // coroutineResume().  coroutineResume() pushed the new coroutine (the one
  // we're in the middle of constructing that was declared as "me" above) onto
  // the running list before returning control to us.  So the return point
  // we're about to set is for ourself.  The call to coroutineAllocateStack here
  // will allocate the next Coroutine on the idle list to be used in the next
  // call to the constructor.
  Coroutine *running = _globalRunning;
  int stackSize = _globalStackSize;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    running = (Coroutine*) tss_get(_tssRunning);
    stackSize = (int) ((intptr_t) tss_get(_tssStackSize));
  }
#endif
  if (!setjmp(running->context)) {
    coroutineAllocateStack(stackSize, NULL);
  }

  if (setjmp(running->resetContext)) {
    // When a coroutine is killed, its normal context is set to this position
    // so that it can be restarted properly from the constructor.  We'll have to
    // manually pull the data that was provided from coroutinePass since the
    // constructor will be thinking that it just provided us with the function
    // we should call.
    funcData = running->passed;
    func = funcData.func;
  }

  // We have just been passed execution from the coroutinePass() statement
  // above.  The stack is now configured and we're ready to begin execution.
  // We will first yield the Coroutine allocated above that the constructor is
  // waiting on and then call the function we were passed.  When the function
  // ends, we will place ourselves on the idle list and can be reused by a
  // future invocation of the coroutineCreate() constructor.
  while (1) {
    // Return our Coroutine and get the function argument from the constructor.
    // coroutineYield will set our state to BLOCKED on call and RUNNING on
    // return.
    void* callingArgument = coroutineYield(&me);

    // Call the target function with the calling argument.
    void* ret = func(callingArgument);

    // Deallocate the currently running coroutine and make it available to the
    // next iteration of the constructor.
#ifdef THREAD_SAFE_COROUTINES
    Coroutine *currentCoroutine = NULL;
    if (!_coroutineThreadingSupportEnabled) {
      currentCoroutine = coroutineGlobalPop(&_globalRunning);
    } else {
      currentCoroutine = coroutineTssPop(&_tssRunning);
    }
#else
    Coroutine* currentCoroutine = coroutineGlobalPop(&_globalRunning);
#endif
    currentCoroutine->id = COROUTINE_ID_NOT_SET;
    currentCoroutine->state = COROUTINE_STATE_NOT_RUNNING;
#ifdef THREAD_SAFE_COROUTINES
    if (!_coroutineThreadingSupportEnabled) {
      coroutineGlobalPush(&_globalIdle, currentCoroutine);
    } else {
      coroutineTssPush(&_tssIdle, currentCoroutine);
    }
#else
    coroutineGlobalPush(&_globalIdle, currentCoroutine);
#endif

    // Block until we're called from the constructor again.
    funcData.data = ret;
    funcData = coroutinePass(&me, funcData);
    func = funcData.func;
  }
}

/// void coroutineAllocateStack(int stackSize, void *topOfStack)
///
/// @brief Allocate space for the current stack to grow before creating the
/// initial stack frame for the next coroutine.
///
/// @return This function returns no value.
void coroutineAllocateStack(int stackSize, void *topOfStack) {
  ZEROINIT(char stack[COROUTINE_STACK_CHUNK_SIZE]);
  
  if (topOfStack == NULL) {
    topOfStack = stack;
  }
  
  if (stackSize > COROUTINE_STACK_CHUNK_SIZE) {
    coroutineAllocateStack(stackSize - COROUTINE_STACK_CHUNK_SIZE, topOfStack);
  }
  
  coroutineMain(topOfStack);
}

/// @fn int coroutineTerminate(Coroutine *targetCoroutine, Comutex **mutexes)
///
/// @brief Kill a coroutine that's currently in progresss.
///
/// @param targetCoroutine A pointer to the Coroutine to kill.
/// @param mutextes A one-dimensional, NULL-terminated array of mutexes to check
///   and unlock if they're locked by the Coroutine.
///
/// @note A Coroutine can only be blocked on a single Cocondition and the
/// information for that Cocondition is contained in the Coroutine structure, so
/// there's no need to pass in Coconditions to check.
///
/// @return Returns coroutineSuccess on success, coroutineError on error.
int coroutineTerminate(Coroutine *targetCoroutine, Comutex **mutexes) {
  if (targetCoroutine == NULL) {
    return coroutineError;
  }

  if (targetCoroutine->state == COROUTINE_STATE_NOT_RUNNING) {
    // It's not possible to take any action on this coroutine.  This is not an
    // error condition because the desired state is achieved.
    return coroutineSuccess;
  }

  // Remove the target coroutine from the running stack if applicable.
  Coroutine* running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif
  if (running == NULL) {
    // running stack hasn't been setup yet.  Bail.
    return coroutineError;
  }

  Coroutine* prev = NULL;
  for (; running != NULL; running = running->next) {
    if (running == targetCoroutine) {
      if (prev != NULL) {
        prev->next = targetCoroutine->next;
      } else {
        // The target coroutine is the top of the running stack.
#ifdef THREAD_SAFE_COROUTINES
        if (!_coroutineThreadingSupportEnabled) {
          coroutineGlobalPop(&_globalRunning);
        } else {
          coroutineTssPop(&_tssRunning);
        }
#else
        coroutineGlobalPop(&_globalRunning);
#endif
      }
      break;
    }
    prev = running;
  }

  // Halt the coroutine.
  targetCoroutine->id = COROUTINE_ID_NOT_SET;
  targetCoroutine->state = COROUTINE_STATE_NOT_RUNNING;
  memcpy(&targetCoroutine->context, &targetCoroutine->resetContext, sizeof(jmp_buf));
#ifdef THREAD_SAFE_COROUTINES
  if (!_coroutineThreadingSupportEnabled) {
    coroutineGlobalPush(&_globalIdle, targetCoroutine);
  }
  else {
    coroutineTssPush(&_tssIdle, targetCoroutine);
  }
#else
  coroutineGlobalPush(&_globalIdle, targetCoroutine);
#endif

  // Unlock any mutexes the coroutine had locked.
  if (mutexes != NULL) {
    for (int i = 0; mutexes[i] != NULL; i++) {
      if (mutexes[i]->coroutine == targetCoroutine) {
        // Unlock the mutex.
        mutexes[i]->recursionLevel = 0;
        mutexes[i]->coroutine = NULL;
      }
    }
  }

  // Remove the coroutine from any condition it was waiting on.
  if (targetCoroutine->prevToSignal != NULL) {
    targetCoroutine->prevToSignal->nextToSignal = targetCoroutine->nextToSignal;
  }
  if (targetCoroutine->nextToSignal != NULL) {
    targetCoroutine->nextToSignal->prevToSignal = targetCoroutine->prevToSignal;
  }

  return coroutineSuccess;
}

/// @fn int coroutineSetId(Coroutine* coroutine, int64_t id)
///
/// @brief Set the ID associated with a coroutine.
///
/// @param coroutine A pointer to the coroutine whose ID is to be set.  If this
///   value is NULL then the ID of the currently running coroutine will be set.
/// @param id A 64-bit signed integer to set as the coroutine's ID.
///
/// @return This function always returns coroutineSuccess.
int coroutineSetId(Coroutine* coroutine, int64_t id) {
  if (coroutine == NULL) {
    coroutine = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
    if (_coroutineThreadingSupportEnabled) {
      call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
      if (!coroutineInitializeThreadMetadata(NULL)) {
        return coroutineError;
      }
      coroutine = (Coroutine*) tss_get(_tssRunning);
    }
#endif
    // running should always be non-NULL, so we shouldn't need to check again.
  }

  if (coroutine == NULL) {
    // Request to set the ID of the currently running Coroutine and there isn't
    // one.  Bail.
    return coroutineError;
  }

  coroutine->id = id;

  return coroutineSuccess;
}

/// @fn int64_t coroutineId(Coroutine* coroutine)
///
/// @brief Get the ID associated with a coroutine.
///
/// @param coroutine A pointer to the coroutine of interest.  If this value is
///   NULL then the ID of the currently running coroutine will be returned.
///
/// @return Returns the ID of the specified or current coroutine.  The ID
/// returned will be COROUTINE_ID_NOT_SET if the ID of the coroutine has not
/// been previously set with a call to coroutineSetId.
int64_t coroutineId(Coroutine* coroutine) {
  if (coroutine == NULL) {
    coroutine = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
    if (_coroutineThreadingSupportEnabled) {
      call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
      if (!coroutineInitializeThreadMetadata(NULL)) {
        return coroutineError;
      }
      coroutine = (Coroutine*) tss_get(_tssRunning);
    }
#endif
    // running should always be non-NULL, so we shouldn't need to check again.
  }

  if (coroutine == NULL) {
    return COROUTINE_ID_NOT_SET;
  }

  return coroutine->id;
}

/// @fn CoroutineState coroutineState(Coroutine* coroutine)
///
/// @brief Get the state of a specified coroutine.
///
/// @param coroutine The Coroutine to examine.
///
/// @return Returns the state of the coroutine on success, NOT_RUNNING if the
/// provided pointer is NULL.
CoroutineState coroutineState(Coroutine* coroutine) {
  CoroutineState state = COROUTINE_STATE_NOT_RUNNING;

  if (coroutine != NULL) {
    state = coroutine->state;
  }

  return state;
}

#ifdef THREAD_SAFE_COROUTINES

/// @fn void coroutineSetThreadingSupportEnabled(bool state)
///
/// @brief Get the current value of the file-local
/// _coroutineThreadingSupportEnabled state variable.  This enables or disables
/// threading support at runtime on systems that support threading.
///
/// @return This function returns no value.
void coroutineSetThreadingSupportEnabled(bool state) {
  _coroutineThreadingSupportEnabled = state;
}

/// @fn bool coroutineThreadingSupportEnabled()
///
/// @brief Get the current value of the file-local
/// _coroutineThreadingSupportEnabled state variable.
///
/// @return Returns the value of _coroutineThreadingSupportEnabled.
bool coroutineThreadingSupportEnabled() {
  return _coroutineThreadingSupportEnabled;
}

#endif // THREAD_SAFE_COROUTINES

/// @fn int coroutineConfig(Coroutine *first, int stackSize)
///
/// @brief Configure the global or thread-specific defaults for all coroutines
/// allocated by the current thread.
///
/// @param first A pointer to the root Coroutine to use.
/// @param stackSize The desired minimum size of a coroutine's stack, in bytes.
///   If this value is less than COROUTINE_STACK_CHUNK_SIZE,
///   COROUTINE_DEFAULT_STACK_SIZE will be used.
///
/// @return Returns coroutineSuccess on success, coroutineError on error.
int coroutineConfig(Coroutine *first, int stackSize) {
  if (stackSize < COROUTINE_STACK_CHUNK_SIZE) {
    stackSize = COROUTINE_DEFAULT_STACK_SIZE;
  }

  Coroutine* idle = _globalIdle;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    idle = (Coroutine*) tss_get(_tssIdle);
  }
#endif // THREAD_SAFE_COROUTINES

  if (idle != NULL) {
    fprintf(stderr,
      "coroutineConfig called after coroutine creation.\n");
    fprintf(stderr, "Cannot execute coroutineConfig.\n");
    return coroutineError;
  }

  // If we made it this far, we're allowed to configure coroutines for this
  // thread.
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    if (tss_get(_tssFirst) != NULL) {
      // croutineConfig weas already called once.  Everything has already been
      // configured, so we just need to reset _tssFirst and _tssRunning.
      if (first != NULL) {
        int status = tss_set(_tssFirst, first);
        if (status != thrd_success) {
          fprintf(stderr,
            "Could not set _tssFirst to first in coroutineConfig.\n");
          return coroutineError;
        }
        status = tss_set(_tssRunning, first);
        if (status != thrd_success) {
          fprintf(stderr,
            "Could not set _tssRunning to first in coroutineConfig.\n");
          return coroutineError;
        }
      }
    } else if (!coroutineInitializeThreadMetadata(first)) {
      fprintf(stderr,
        "Could not initialize thread metadata in coroutineConfig.\n");
        return coroutineError;
    }
    tss_set(_tssStackSize, (void*) ((intptr_t) stackSize));
  }
#endif // THREAD_SAFE_COROUTINES
  if (first != NULL) {
    memset(first, 0, sizeof(Coroutine));
    _globalFirst = first;
    _globalRunning = first;
  } else if (_globalFirst == NULL) {
    fprintf(stderr,
      "NULL first Coroutine provided and no first Coroutine set.\n");
    return coroutineError;
  }
  _globalStackSize = stackSize;

  return coroutineSuccess;
}

/// @fn int comutexInit(Comutex* mtx, int type)
///
/// @brief Initialize a coroutine mutex.
///
/// @param mtx A pointer to the allocated coroutine mutex to initialize.
/// @param type A bitwise-ored integer of the type of mutex this is to be.
///
/// @return This function always returns coroutine success in the current
/// implementation.
int comutexInit(Comutex *mtx, int type) {
  int returnValue = coroutineSuccess;

  if (mtx != NULL) {
    mtx->lastYieldValue = NULL;
    mtx->type = type;
    mtx->coroutine = NULL;
    mtx->recursionLevel = 0;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int comutexLock(Comutex* mtx)
///
/// @brief Lock a coroutine mutex.
///
/// @details This function blocks the current coroutine, yielding each time it
/// tries and fails to acquire the lock.  The special value COROUTINE_BLOCKED
/// will be yielded to the caller each time control is yielded.
///
/// @param mtx A pointer to the coroutine mutex to lock.
///
/// @return Returns coroutineSuccess when the lock is acquired, coroutineError
/// if the provided pointer is NULL.
int comutexLock(Comutex *mtx) {
  if (mtx == NULL) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  mtx->lastYieldValue = NULL;

  while (comutexTrylock(mtx) != coroutineSuccess) {
    mtx->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
  }

  return coroutineSuccess;
}

/// @fn int comutexUnlock(Comutex* mtx)
///
/// @brief Unlock a previously-locked coroutine mutex.
///
/// @param mtx A pointer to the he coroutine mutex to unlock.
///
/// @return Returns coroutineSuccess if the currently-running coroutine has the
/// lock, coroutineError otherwise.  If the currently-running coroutine has the
/// lock and the depth of the unlock calls matches the depth of the depth of the
/// lock calls (mtx->recursionLevel reaches 0 with this call), the mutex is
/// unlocked.
int comutexUnlock(Comutex *mtx) {
  int returnValue = coroutineSuccess;

  Coroutine* running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return coroutineError;
    }
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif

  if (running == NULL) {
    // running stack not setup yet.  Bail.
    return coroutineError;
  }

  if ((mtx != NULL) && (mtx->coroutine == running)) {
    mtx->recursionLevel--;
    if (mtx->recursionLevel == 0) {
      mtx->coroutine = NULL;
    }
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn void comutexDestroy(Comutex* mtx)
///
/// @brief Destroy a previously-initialized coroutine mutex.
///
/// @param mtx A pointer to the mutex to destory.
///
/// @return This function returns no value.
void comutexDestroy(Comutex *mtx) {
  if (mtx != NULL) {
    mtx->lastYieldValue = NULL;
    mtx->type = 0;
    mtx->coroutine = NULL;
    mtx->recursionLevel = 0;
  }
}

/// @fn int comutexTimedlock(Comutex* mtx, const struct timespec* ts)
///
/// @brief Attempt to lock a coroutine mutex until the lock is acquired or a
/// specified time is reached, whichever comes first.
///
/// @param mtx A pointer to the mutex to destory.
/// @param ts A pointer to a struct timespec instance that specifies the future
///   deadline for abandoning attempts to lock the mutex.
///
/// @return Returns coroutineSuccess if the lock is acquired before the timeout
/// is reached, coroutineTimeout if the timeout is reached before the lock is
/// acquired, and coroutineError if the coroutine mutex is not a timed mutex,
/// if the current system time could not be acquired, or if one of the provided
/// parameters is NULL.
int comutexTimedlock(Comutex *mtx, const struct timespec *ts) {
  if ((mtx == NULL) || (ts == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  mtx->lastYieldValue = NULL;

  if (!(mtx->type & comutexTimed)) {
    // This is not a timed mutex.  It does not support timeouts.  We fail.
    return coroutineError;
  }

  int returnValue = comutexTrylock(mtx);
  while (returnValue != coroutineSuccess) {
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == 0) {
      uint64_t nowns = (now.tv_sec * 1000000000) + now.tv_nsec;
      uint64_t tsns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
      if (nowns > tsns) {
        returnValue = coroutineTimedout;
        break;
      }
      mtx->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
      returnValue = comutexTrylock(mtx);
    } else {
      // timespec_get returned an error.  We have no valid time to wait.  We've
      // already tried to lock once and that's the best we can do.
      returnValue = coroutineError;
      break;
    }
  }

  return returnValue;
}

/// @fn int comutexTrylock(Comutex* mtx)
///
/// @brief Make one attempt to lock a coroutine mutex.
///
/// @param mtx A pointer to the coroutine mutex to attempt to lock.
///
/// @return Returns coroutineSuccess if the mutex is unlocked or if the current
/// coroutine has the lock and the mutex is recursive, coroutineBusy if the
/// mutex is locked by antoher coroutine, and coroutineError under any other
/// conditions.
int comutexTrylock(Comutex *mtx) {
  if (mtx == NULL) {
    // Cannot honor the request.
    return coroutineError;
  }

  int returnValue = coroutineError;

  Coroutine* running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return coroutineError;
    }
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif

  if (running == NULL) {
    // running stack not setup yet.  Bail.
    return coroutineError;
  }

  if (mtx->coroutine == NULL) {
    mtx->coroutine = running;
    mtx->recursionLevel = 1;
    returnValue = coroutineSuccess;
  } else if ((mtx->coroutine == running) && (mtx->type & comutexRecursive)) {
    mtx->recursionLevel++;
    returnValue = coroutineSuccess;
  } else if (mtx->coroutine != running) {
    returnValue = coroutineBusy;
  } // else any other situation is an error, which is the value of returnValue

  return returnValue;
}

/// @fn void* comutexLastYieldValue(Comutex* mtx)
///
/// @brief Get the last value returned by a yield call in a blocking comutex
/// lock function.
///
/// @param mtx A pointer to the comutex to interrogate.
///
/// @return Returns the last value returned by a blocking comutex lock function
/// performed on the specified if there is such a value.  NULL if the lock
/// function succeeded without yielding or if the provided comutex is NULL.
void* comutexLastYieldValue(Comutex* mtx) {
  void *returnValue = NULL;

  if (mtx != NULL) {
    returnValue = mtx->lastYieldValue;
  }

  return returnValue;
}

/// @fn int coconditionBroadcast(Cocondition *cond)
///
/// @brief Broadcast a condition to all coroutines blocked on it.
///
/// @param cond A pointer to the coroutine condition to broadcast.
///
/// @return Returns coroutineSuccess on success, coroutineError if the call
/// could not be honored (cond is NULL).
int coconditionBroadcast(Cocondition *cond) {
  int returnValue = coroutineSuccess;

  if (cond != NULL) {
    cond->numSignals = cond->numWaiters;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn void coconditionDestroy(Cocondition* cond)
///
/// @brief Destroy a previously initialized coroutine condition.
///
/// @param cond A pointer to the coroutine condition to destroy.
///
/// @return This function returns no value.
void coconditionDestroy(Cocondition *cond) {
  if (cond != NULL) {
    cond->lastYieldValue = NULL;
    cond->numWaiters = 0;
    cond->numSignals = -1;
  }
}

/// @fn int coconditionInit(Cocondition* cond)
///
/// @brief Initialize a coroutine condition variable.
///
/// @param cond A pointer to the coroutine condition to destroy.
///
/// @return Returns coroutineSuccess on successful initialization,
/// coroutineError if the request could not be honored (cond is NULL).
int coconditionInit(Cocondition* cond) {
  int returnValue = coroutineSuccess;

  if (cond != NULL) {
    cond->lastYieldValue = NULL;
    cond->numWaiters = 0;
    cond->numSignals = 0;
    cond->head = NULL;
    cond->tail = NULL;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int coconditionSignal(Cocondition *cond)
///
/// @brief Signal a single coroutiune blocked on a condition.
///
/// @param cond A pointer to the coroutine condition to signal.
///
/// @return Returns coroutineSuccess on successful signalling, coroutineError
/// if the call could not be honored (cond is NULL).
int coconditionSignal(Cocondition *cond) {
  int returnValue = coroutineSuccess;

  if ((cond != NULL) && (cond->numWaiters > 0)) {
    cond->numSignals++;
  } else {
    returnValue = coroutineError;
  }

  return returnValue;
}

/// @fn int conditionTimedwait(Cocondition* cond, Comutex* mtx, const struct timespec* ts)
///
/// @brief Wait for a condition to be signalled or until a specified time,
/// whichever comes first.
///
/// @param cond A pointer to the condition to wait on.
/// @param mtx A mutex for the condition that must be locked before this call
///   is made.  It will be unlocked before blocking on the condition and locked
///   again before the function returns.
/// @param ts A struct timespec that specifies the future deadline of the wait.
///
/// @return Returns coroutineSuccess on success, coroutineTimedout if the
/// deadline is reached before the condition is signalled, or coroutineError
/// if the request could not be honored (a parameter is NULL or timespec_get
/// fails).
int conditionTimedwait(Cocondition *cond, Comutex *mtx,
  const struct timespec *ts
) {
  if ((cond == NULL) || (mtx == NULL) || (ts == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  cond->lastYieldValue = NULL;

  comutexUnlock(mtx);

  Coroutine* running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return coroutineError;
    }
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif

  if (running == NULL) {
    // running stack not setup yet.  Bail.
    return coroutineError;
  }

  // Add ourselves to the queue.
  cond->numWaiters++;
  if (cond->tail != NULL) {
    cond->tail->nextToSignal = running;
  }
  running->prevToSignal = cond->tail;
  cond->tail = running;
  if (cond->head == NULL) {
    cond->head = running;
  }

  int returnValue = coroutineSuccess;
  while ((cond->numSignals == 0) || (cond->head != running)) {
    cond->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);

    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == 0) {
      uint64_t nowns = (now.tv_sec * 1000000000) + now.tv_nsec;
      uint64_t tsns = (ts->tv_sec * 1000000000) + ts->tv_nsec;
      if (nowns > tsns) {
        returnValue = coroutineTimedout;
        break;
      }
    } else if (cond->numSignals == 0) {
      // timespec_get returned an error.  We have no valid time to wait.  We've
      // already tried to wait once and that's the best we can do.
      returnValue = coroutineError;
      break;
    }
  }
  if ((returnValue == coroutineSuccess) && (cond->numSignals > 0)) {
    cond->numSignals--;
    cond->numWaiters--;
    if (running->prevToSignal != NULL) {
      running->prevToSignal->nextToSignal = running->nextToSignal;
    } else {
      // We are the head of the queue.
      cond->head = running->nextToSignal;
    }
    if (running->nextToSignal != NULL) {
      running->nextToSignal->prevToSignal = running->prevToSignal;
    } else {
      // We are the tail of the queue;
      cond->tail = running->prevToSignal;
    }
    returnValue = coroutineSuccess;
  } else if (returnValue == coroutineTimedout) {
    // Remove ourselves from the queue.  We could be anywhere in the queue, so
    // manage the links accordingly.
    if (running->prevToSignal != NULL) {
      running->prevToSignal->nextToSignal = running->nextToSignal;
    } else {
      // We are the head node.
      cond->head = running->nextToSignal;
    }
    if (cond->tail == running) {
      cond->tail = running->prevToSignal;
    }
    cond->numWaiters--;
  } else {
    // The condition has been destroyed out from under us.  Invalid state.
    returnValue = coroutineError;
  }

  comutexLock(mtx);
  return returnValue;
}

/// @fn int coconditionWait(Cocondition* cond, Comutex* mtx)
///
/// @brief Wait for the specified condition to be signalled.
///
/// @param cond A pointer to the condition to wait on.
/// @param mtx A mutex for the condition that must be locked before this call
///   is made.  It will be unlocked before blocking on the condition and locked
///   again before the function returns.
///
/// @return Returns coroutineSuccess on success or coroutineError if the request
/// could not be honored (one or more NULL parameters).
int coconditionWait(Cocondition *cond, Comutex *mtx) {
  if ((cond == NULL) || (mtx == NULL)) {
    // Cannot honor the request.
    return coroutineError;
  }

  // Clear the lastYieldValue before we do anything else.
  cond->lastYieldValue = NULL;

  comutexUnlock(mtx);

  Coroutine* running = _globalRunning;
#ifdef THREAD_SAFE_COROUTINES
  if (_coroutineThreadingSupportEnabled) {
    call_once(&_threadMetadataSetup, coroutineSetupThreadMetadata);
    if (!coroutineInitializeThreadMetadata(NULL)) {
      return coroutineError;
    }
    running = (Coroutine*) tss_get(_tssRunning);
  }
#endif

  if (running == NULL) {
    // running stack not setup yet.  Bail.
    return coroutineError;
  }

  // Add ourselves to the queue.
  cond->numWaiters++;
  if (cond->tail != NULL) {
    cond->tail->nextToSignal = running;
  }
  running->prevToSignal = cond->tail;
  cond->tail = running;
  if (cond->head == NULL) {
    cond->head = running;
  }

  int returnValue = coroutineSuccess;
  while ((cond->numSignals == 0) || (cond->head != running)) {
    cond->lastYieldValue = coroutineYield(COROUTINE_BLOCKED);
  }
  if (cond->numSignals > 0) {
    cond->numSignals--;
    cond->numWaiters--;
    cond->head = running->nextToSignal;
    if (running->prevToSignal != NULL) {
      running->prevToSignal->nextToSignal = running->nextToSignal;
    } else {
      // We are the head of the queue.
      cond->head = running->nextToSignal;
    }
    if (running->nextToSignal != NULL) {
      running->nextToSignal->prevToSignal = running->prevToSignal;
    } else {
      // We are the tail of the queue;
      cond->tail = running->prevToSignal;
    }
  } else {
    // The condition has been destroyed out from under us.  Invalid state.
    returnValue = coroutineError;
  }

  comutexLock(mtx);
  return returnValue;
}

/// @fn void* coconditionLastYieldValue(Cocondition* cond)
///
/// @brief Retrieve the last value yielded to a condition wait call.
///
/// @returns The last value yielded on the conditon on success, NULL if the
/// provided condition pointer is NULL.
void* coconditionLastYieldValue(Cocondition* cond) {
  void *returnValue = NULL;

  if (cond != NULL) {
    returnValue = cond->lastYieldValue;
  }

  return returnValue;
}

