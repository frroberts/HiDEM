//
// Created by Fredrik Robertsen on 02/04/2022.
//

#ifndef PHELL_HIDEMGLOBAL_H
#define PHELL_HIDEMGLOBAL_H

#ifdef SET_CT_FP32
typedef float ctReal_t;
#else
typedef double ctReal_t;
#endif

#define maxLinks 13

#endif //PHELL_HIDEMGLOBAL_H
