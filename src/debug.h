#ifndef DEBUG_H_
#define DEBUG_H_

#if DEBUG

#if !defined(DEBUG_VERBOSE)
#define DEBUG_VERBOSE 0
#endif

void debug_init();
void debug_task();
#define DEBUG_INIT() debug_init()
#define DEBUG_TASK() debug_task()

void dbg(const char* tag, const char *fmt, ...);

#ifndef DEBUG_TAG
#define DEBUG_TAG "??"
#endif

#define DBG(...) dbg(DEBUG_TAG, __VA_ARGS__)
#if DEBUG_VERBOSE > 0
#define DBG_V(...) dbg(DEBUG_TAG, __VA_ARGS__)
#endif
#if DEBUG_VERBOSE > 1
#define DBG_VV(...) dbg(DEBUG_TAG, __VA_ARGS__)
#endif
#if DEBUG_VERBOSE > 2
#define DBG_VVV(...) dbg(DEBUG_TAG, __VA_ARGS__)
#endif
#define DBG_CONT(...) dbg(nullptr, __VA_ARGS__)

#else

#define DEBUG_INIT() do { } while (0)
#define DEBUG_TASK() do { } while (0)
#define DBG(...) do { } while (0)
#define DBG_CONT(...) do { } while (0)

#endif

#ifndef DBG_V
#define DBG_V(...) do { } while (0)
#endif
#ifndef DBG_VV
#define DBG_VV(...) do { } while (0)
#endif
#ifndef DBG_VVV
#define DBG_VVV(...) do { } while (0)
#endif

#endif