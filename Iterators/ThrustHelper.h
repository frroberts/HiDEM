//
// Created by Fredrik Robertsen on 5/4/25.
//

#ifndef THRUSTHELPER_H
#define THRUSTHELPER_H

#include "../PLiB/Global.h"

#ifdef GPU

#include <thrust/transform_reduce.h>
#include <thrust/functional.h>
#include <thrust/execution_policy.h>
#endif

namespace ParStd
{
#ifdef GPU
    /*
    Fixes missmatch between thrust and stl
    STL interface is
     template< class ExecutionPolicy,

          class ForwardIt, class T, class BinaryOp, class UnaryOp >
            T transform_reduce( ExecutionPolicy&& policy,
                    ForwardIt first, ForwardIt last, T init,
                    BinaryOp reduce, UnaryOp transform );
    Native thrust is
    template<typename DerivedPolicy, typename InputIterator, typename UnaryFunction, typename OutputType, typename BinaryFunction>
        OutputType thrust::transform_reduce(const thrust::detail::execution_policy_base<DerivedPolicy> &exec,
        InputIterator first, InputIterator last, UnaryFunction unary_op, OutputType init, BinaryFunction binary_op)
    */
    template< class ExecutionPolicy,
        class ForwardIt, class T, class BinaryOp, class UnaryOp >
            T transform_reduce( ExecutionPolicy&& policy,
                    ForwardIt first, ForwardIt last, T init,
                    BinaryOp reduce, UnaryOp transform)
    {
        return thrust::transform_reduce(policy, first, last, transform, init, reduce);
    }
#else
    template< class ExecutionPolicy,
        class ForwardIt, class T, class BinaryOp, class UnaryOp >
            T transform_reduce( ExecutionPolicy&& policy,
                    ForwardIt first, ForwardIt last, T init,
                    BinaryOp reduce, UnaryOp transform)
    {
        return std::transform_reduce(policy, first, last, init, reduce,transform);
    }


    template<
        class ForwardIt, class T, class BinaryOp, class UnaryOp >
            T transform_reduce(
                    ForwardIt first, ForwardIt last, T init,
                    BinaryOp reduce, UnaryOp transform)
    {
        return std::transform_reduce(first, last, init, reduce,transform);
    }
#endif
}

#endif //THRUSTHELPER_H
