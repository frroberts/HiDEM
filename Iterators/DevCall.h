//
// Created by Fredrik Robertsen on 07/11/2020.
//

#ifndef PHELL_DEVCALL_H
#define PHELL_DEVCALL_H

#include "../PLiB/Global.h"
#include <signal.h>

#ifdef ENABLE_STACKTRACE
#include <stacktrace>
#endif

#ifdef ENABLE_UNWIND
#include "../PLiB/Util/StackTrace.h"
#endif

#if ( ENABLE_CUDA == 1) || ( ENABLE_HIP == 1)
#define DEVCALLABLE __host__ __device__
#define DEVLAMBDA __device__
#else
#define DEVCALLABLE
#define DEVLAMBDA
#endif

// #define DEBUG_DEV

#ifdef DEBUG_DEV

#if defined(ENABLE_UNWIND)
#define getErrorDev(command)\
	command;\
	devDeviceSynchronize();\
	if (devPeekAtLastError() != devSuccess){ \
		std::cout << #command << " : " << devGetErrorString(devGetLastError())\
			<< " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
		printStacktraceUnwind(std::cout);\
		raise(SIGINT);      \
		exit((1));\
	}
#elif defined (ENABLE_STACKTRACE)
#define getErrorDev(command)\
	command;\
	devDeviceSynchronize();\
	if (devPeekAtLastError() != devSuccess){ \
		std::cout << #command << " : " << devGetErrorString(devGetLastError())\
			<< " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
		std::cout << std::stacktrace::current() << std::endl;\
		raise(SIGINT);      \
		exit((1));\
	}
#else
#define getErrorDev(command)\
	command;\
	devDeviceSynchronize();\
	if (devPeekAtLastError()!= devSuccess){ \
		std::cout << #command << " : " << devGetErrorString(devGetLastError())\
		<< " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
		raise(SIGINT);      \
		exit((1));\
	}
#endif
#else
#define getErrorDev(command) command;

#endif

#if defined(ENABLE_UNWIND)
#define getErrorDevApi(command)\
{\
auto ret = command;\
if (ret != devSuccess){ \
std::cout << #command << " : " << devGetErrorString(ret)\
<< " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
printStacktraceUnwind(std::cout);\
raise(SIGINT);      \
exit((1));\
}}
#elif defined (ENABLE_STACKTRACE)
#else
#define getErrorDevApi(command)\
{\
auto ret = command;\
if (ret != devSuccess){ \
std::cout << #command << " : " << devGetErrorString(ret)\
<< " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
raise(SIGINT);      \
exit((1));\
}}
#endif


#endif
