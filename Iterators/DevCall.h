//
// Created by Fredrik Robertsen on 07/11/2020.
//

#ifndef PHELL_DEVCALL_H
#define PHELL_DEVCALL_H

#include "../PLiB/Global.h"



#if ( ENABLE_CUDA == 1) || ( ENABLE_HIP == 1)
#define DEVCALLABLE __host__ __device__
#define DEVLAMBDA __device__
#else
#define DEVCALLABLE
#define DEVLAMBDA
#endif

//#define DEBUG_DEV
#ifdef DEBUG_DEV
#define getErrorDev(command)\
		command;\
		devDeviceSynchronize();\
		if (devPeekAtLastError() != devSuccess){ \
            std::cout << #command << " : " << devGetErrorString(devGetLastError())\
			 << " in file " << __FILE__ << " at line " << __LINE__ << " in function " << __PRETTY_FUNCTION__ << std::endl;\
			raise(SIGINT);      \
             exit((1));\
		}
#endif
#ifndef DEBUG_DEV
#define getErrorDev(command) command;
#endif

#endif
