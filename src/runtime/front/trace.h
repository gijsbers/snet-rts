#ifndef _TRACE_H
#define _TRACE_H

/* Only trace when ENABLE_TRACE is defined externally in the Makefile */
#if ENABLE_TRACE
    extern void (*SNetTraceFunc)(const char *func);
#   define trace(x)     (*SNetTraceFunc)(x)
#else
#   define trace(x)     /*nothing*/
#endif



/* Show a function name and a small stacktrace. */
void SNetTraceCalls(const char *func);

/* Trace only the function name. */
void SNetTraceSimple(const char *func);

/* Trace nothing. */
void SNetTraceEmpty(const char *func);

/* Set tracing: -1 disables, >= 0 sets call stack tracing depth. */
void SNetEnableTracing(int trace_level);

/* Force tracing of a function call at a specific depth. */
void SNetTrace(const char *func, int trace_level);

#endif
