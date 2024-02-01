#include "ProfileTrace.h"
#include <cstdio>
#include <dlfcn.h>
#include "Common.h"

#define MAX_BUFFER_SIZE 256

void *(*txrTraceEnterInternal) (const char* sectionName);
void *(*txrTraceExitInternal) (void);
static bool traceEnabled = false;
typedef void *(*fp_ATrace_beginSection) (const char* sectionName);
typedef void *(*fp_ATrace_endSection) (void);
typedef void *(*fp_ATrace_isEnabled) (void);

void initializeATrace() {
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib != NULL) {
        txrTraceEnterInternal = reinterpret_cast<fp_ATrace_beginSection>(dlsym(lib, "ATrace_beginSection"));
        txrTraceExitInternal = reinterpret_cast<fp_ATrace_endSection>(dlsym(lib, "ATrace_endSection"));
        char pValue[32];
        bool find = __system_property_get("persist.sys.txr_enable_trace", pValue) >= 1;
        LOG_E("---------> Profile trace enabled: %d", find);
        if (find) {
            traceEnabled = atoi(pValue) ? true : false;
        }
    }
}

void TRACE_BEGIN(const char* fmt, ...) {
    if (traceEnabled) {
        va_list args;
        va_start(args, fmt);
        char buffer[MAX_BUFFER_SIZE];
        vsnprintf(buffer, MAX_BUFFER_SIZE, fmt, args);
        va_end(args);
        txrTraceEnterInternal(buffer);
    }
}

void TRACE_END(const char* fmt, ...) {
    if (traceEnabled) {
        txrTraceExitInternal();
    }
}
