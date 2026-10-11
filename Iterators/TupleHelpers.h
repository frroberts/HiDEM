//
// Created by Fredrik Robertsen on 5/1/24.
//

#ifndef PHELL_TUPLEHELPERS_H
#define PHELL_TUPLEHELPERS_H

#include <tuple>
#include <stdint.h>
#include <DevCall.h>
#include "ManVec.h"


template<class T>
DEVCALLABLE
T &getAtInd(int index, T *vec) {
    return vec[index];
}

template<class T>
DEVCALLABLE
T &getAtIndVec(int index, ManVec<T> *vec) {
    return (*vec)[index];
}

template<class T>
DEVCALLABLE
void setAtInd(int index, T *vec, T val) {
    vec[index] = val;
}

//implementation.
template<class Fun, class ...Args, std::size_t... Is>
DEVCALLABLE
void callFunImpl(Fun fun, std::tuple<Args...> &tuple,
              std::index_sequence<Is...>) {
    fun(std::get<Is>(tuple)...);
}

// the helper functions
template<class Fun, class ...TupleArgs>
DEVCALLABLE
void callFun(Fun fun, std::tuple<TupleArgs...> &tuple) {
    callFunImpl(fun, tuple, std::make_index_sequence<sizeof...(TupleArgs)>());
}

//implementation.
template<class Fun,
        class ...Args1, std::size_t... Is1,
        class ...Args2, std::size_t... Is2>
DEVCALLABLE
void callFunImpl2(Fun fun,
                  std::tuple<Args1...> &tuple1, std::index_sequence<Is1...>,
                  std::tuple<Args2...> &tuple2, std::index_sequence<Is2...>) {
    fun(std::get<Is1>(tuple1)..., std::get<Is2>(tuple2)...);
}

// the helper functions
template<class Fun, class ...TupleArgs1, class ...TupleArgs2>
DEVCALLABLE
void callFun2(Fun fun, std::tuple<TupleArgs1...> &tuple1, std::tuple<TupleArgs2...> &tuple2) {
    callFunImpl2(fun,
                 tuple1, std::make_index_sequence<sizeof...(TupleArgs1)>(),
                 tuple2, std::make_index_sequence<sizeof...(TupleArgs2)>());
}

//implementation.
template<class Fun,
        class ...Args1, std::size_t... Is1,
        class ...Args2, std::size_t... Is2,
        class ...Args3, std::size_t... Is3>
DEVCALLABLE
void callFunImpl3(Fun fun,
                  std::tuple<Args1...> &tuple1, std::index_sequence<Is1...>,
                  std::tuple<Args2...> &tuple2, std::index_sequence<Is2...>,
                  std::tuple<Args3...> &tuple3, std::index_sequence<Is3...>) {
    fun(std::get<Is1>(tuple1)..., std::get<Is2>(tuple2)..., std::get<Is3>(tuple3)...);
}

// the helper functions
template<class Fun, class ...TupleArgs1, class ...TupleArgs2, class ...TupleArgs3>
DEVCALLABLE
void callFun3(Fun fun, std::tuple<TupleArgs1...> &tuple1, std::tuple<TupleArgs2...> &tuple2,
              std::tuple<TupleArgs3...> &tuple3) {
    callFunImpl3(fun,
                 tuple1, std::make_index_sequence<sizeof...(TupleArgs1)>(),
                 tuple2, std::make_index_sequence<sizeof...(TupleArgs2)>(),
                 tuple3, std::make_index_sequence<sizeof...(TupleArgs3)>());
}

//implementation.
template<class Fun,
        class ...Args1, std::size_t... Is1,
        class ...Args2, std::size_t... Is2,
        class ...Args3, std::size_t... Is3,
                class ...Args4, std::size_t... Is4>
DEVCALLABLE
void callFunImpl4(Fun fun,
                  std::tuple<Args1...> &tuple1, std::index_sequence<Is1...>,
                  std::tuple<Args2...> &tuple2, std::index_sequence<Is2...>,
                  std::tuple<Args3...> &tuple3, std::index_sequence<Is3...>,
                  std::tuple<Args4...> &tuple4, std::index_sequence<Is4...>) {
    fun(std::get<Is1>(tuple1)..., std::get<Is2>(tuple2)..., std::get<Is3>(tuple3)..., std::get<Is4>(tuple4)...);
}

// the helper functions
template<class Fun, class ...TupleArgs1, class ...TupleArgs2, class ...TupleArgs3, class ...TupleArgs4>
DEVCALLABLE
void callFun4(Fun fun,
              std::tuple<TupleArgs1...> &tuple1,
              std::tuple<TupleArgs2...> &tuple2,
              std::tuple<TupleArgs3...> &tuple3,
              std::tuple<TupleArgs4...> &tuple4) {
    callFunImpl4(fun,
                 tuple1, std::make_index_sequence<sizeof...(TupleArgs1)>(),
                 tuple2, std::make_index_sequence<sizeof...(TupleArgs2)>(),
                 tuple3, std::make_index_sequence<sizeof...(TupleArgs3)>(),
                 tuple4, std::make_index_sequence<sizeof...(TupleArgs4)>());
}

template<class ...Args>
DEVCALLABLE
std::tuple<Args...> getAtIndTuple(int ind, std::tuple<Args *...> ptrs) {
    return std::apply([&](auto ...x) {
        return std::make_tuple(getAtInd(ind, x)...);
    }, ptrs);
}

template<class ...Vecs>
DEVCALLABLE
auto getAtIndVecTuple(int ind, std::tuple<Vecs...> &ptrs) {
    return std::apply([&](Vecs ...x) {
        return std::make_tuple(getAtIndVec(ind, x)...);
    }, ptrs);
}


template<class T>
DEVCALLABLE
T *slicePtr(int &offset, T *type, int count, uint8_t *base) {
    //
    T *ptr;
    ptr = (T *) &base[offset];
    offset += sizeof(T) * count;
    return ptr;
}

template<std::size_t... ints, class ...Args>
DEVCALLABLE
std::tuple<Args *...>
makeSharedPtrsImpl(uint8_t *base, int count, std::tuple<Args *...> ptrs, std::integer_sequence<std::size_t, ints...>) {
    int offset = 0;
    return std::apply([&](auto ...x) {
        return std::make_tuple(slicePtr(offset, x, count, base)...);
    }, ptrs);
}

template<class ...Args>
DEVCALLABLE
std::tuple<Args *...>
makeSharedPtrs(uint8_t *base, int count, std::tuple<Args *...> ptrs) {
    return makeSharedPtrsImpl(base, count, ptrs, std::make_index_sequence<sizeof...(Args)>());
}

template<std::size_t... ints, class ...Args>
DEVCALLABLE
void setAtIndTupleImpl(int ind, std::tuple<Args *...> ptrs, std::tuple<Args...> vals,
                   std::integer_sequence<std::size_t, ints...>) {
    ((std::get<ints>(ptrs)[ind] = std::get<ints>(vals)), ...);
}


template<class ...Args>
DEVCALLABLE
void setAtIndTuple(int ind, std::tuple<Args *...> ptrs, std::tuple<Args...> vals) {
    setAtIndTupleImpl(ind, ptrs, vals, std::make_index_sequence<sizeof...(Args)>());
}

template<std::size_t... ints, class ... VecPtrs, class ...Args>
DEVCALLABLE
void setAtIndVecTupleImpl(int ind, std::tuple<VecPtrs ...> ptrs, std::tuple<Args...> vals,
                   std::integer_sequence<std::size_t, ints...>) {
    (((*std::get<ints>(ptrs))[ind] = std::get<ints>(vals)), ...);
}


template<class ... VecPtrs, class ...Args>
DEVCALLABLE
void setAtIndVecTuple(int ind, std::tuple<VecPtrs...> ptrs, std::tuple<Args...> vals) {
    setAtIndVecTupleImpl(ind, ptrs, vals, std::make_index_sequence<sizeof...(Args)>());
}


template<std::size_t... ints, class ...Args>
DEVCALLABLE
void addAtIndTupleImpl(int ind, std::tuple<Args *...> ptrs, std::tuple<Args...> vals,
                   std::integer_sequence<std::size_t, ints...>) {
    ((std::get<ints>(ptrs)[ind] += std::get<ints>(vals)), ...);
}


template<class ...Args>
DEVCALLABLE
void addAtIndTuple(int ind, std::tuple<Args *...> ptrs, std::tuple<Args...> vals) {
    addAtIndTupleImpl(ind, ptrs, vals, std::make_index_sequence<sizeof...(Args)>());
}

template<std::size_t... ints, class ...ArgsA, class ...ArgsB>
auto zipTuplesImpl(std::tuple<ArgsA ...> tupA, std::tuple<ArgsB...> tupB, std::integer_sequence<std::size_t, ints...>) {
    return std::tuple_cat(std::make_tuple(std::get<ints>(tupA), std::get<ints>(tupB))...);
}

template<class ...ArgsA, class ...ArgsB>
auto zipTuples(std::tuple<ArgsA ...> tupA, std::tuple<ArgsB...> tupB) {
    return zipTuplesImpl(tupA, tupB, std::make_index_sequence<sizeof...(ArgsA)>());
}




// make a lambda in zero
template<class T>
DEVCALLABLE
T getZero(T *arg) {
    return T(0);
}
template<class ...Args>
DEVCALLABLE
std::tuple<Args...> zeroTuple(std::tuple<Args *...> args) {
    return std::apply([=](auto ...x) {
        return std::make_tuple(getZero(x)...);
    }, args);
}

template<class ...Vec>
DEVCALLABLE
auto packVecsDataPtrs(Vec& ... vecs){
    return std::make_tuple(vecs.data()...);
}

template<class ...Vec>
DEVCALLABLE
auto packVecPtrs(Vec& ... vecs){
    return std::make_tuple(&vecs...);
}


template<std::size_t... ints, class ...Args>
auto vecPtrToDataPtrImpl(std::tuple<Args ...> ptrs, std::integer_sequence<std::size_t, ints...>) {
    return std::apply([&](Args ...x) {
        return std::make_tuple((x)->data()...);
    }, ptrs);
}

template<class ...Vec>
auto vecPtrToDataPtr(std::tuple<Vec ...> vecs){
    return vecPtrToDataPtrImpl(vecs, std::make_index_sequence<std::tuple_size<std::tuple<Vec ...>>{}>());
}

template<int offset, int... Is, class Args>
constexpr
auto sliceTupleImpl(Args fullArgs, std::integer_sequence<int, Is...>) {
    return std::make_tuple(std::get<offset + Is>(fullArgs)...);
}

template<int N, int offset, class Args>
constexpr
auto sliceTuple(Args fullArgs) {
    return sliceTupleImpl<offset>(fullArgs, std::make_integer_sequence<int, N>{});
}

template<typename ...Elems, typename Fun>
void callForTupleElem(std::tuple<Elems...> elems, Fun fun) {
    callForTupleElemImpl(elems, std::make_index_sequence<sizeof...(Elems)>(), fun);
}

template<std::size_t... ints, typename ...Elems, typename Fun>
void callForTupleElemImpl(std::tuple<Elems...> elems, std::integer_sequence<std::size_t, ints...>, Fun fun) {
    (fun(std::get<ints>(elems)), ...);
}

template<typename ...Elems, typename Fun>
void callForTupleElemRef(std::tuple<Elems...> &elems, Fun fun) {
    callForTupleElemRefImpl(elems, std::make_index_sequence<sizeof...(Elems)>(), fun);
}

template<std::size_t... ints, typename ...Elems, typename Fun>
void callForTupleElemRefImpl(std::tuple<Elems...> &elems, std::integer_sequence<std::size_t, ints...>, Fun fun) {
    (fun(std::get<ints>(elems)), ...);
}


template<typename ...Elems1, typename ...Elems2, typename Fun>
void callForTuple2Elem(std::tuple<Elems1...> elems1, std::tuple<Elems2...> elems2, Fun fun) {
    callForTuple2ElemImpl(elems1, elems2, std::make_index_sequence<sizeof...(Elems1)>(), fun);
}

template<std::size_t... ints, typename ...Elems1, typename ...Elems2, typename Fun>
void callForTuple2ElemImpl(std::tuple<Elems1...> elems1, std::tuple<Elems2...> elems2, std::integer_sequence<std::size_t, ints...>, Fun fun) {
    (fun(std::get<ints>(elems1), std::get<ints>(elems2)), ...);
}



// helper class to get around nvcc not liking multiple parameter packs
// at some point if nvcc improves rewrite to anything using this just passing multiple packs
template<int N1,int N2,int N3, class Args>
struct Pack3{
    const Args fullArgs;
    explicit constexpr
    Pack3(Args args) : fullArgs{args}{
    }

    template<int offset, int... Is>
    constexpr
    auto sliceTuple(std::integer_sequence<int, Is...>) const {
        return std::make_tuple(std::get<offset + Is>(fullArgs)...);
    }

    constexpr
    auto split() const {
    return std::make_tuple(
            sliceTuple<0>(std::make_integer_sequence<int, N1>{}),
            sliceTuple<N1>(std::make_integer_sequence<int, N2>{}),
            sliceTuple<N1+N2>(std::make_integer_sequence<int, N3>{})
            );
    }
};

template <class... Args1, class... Args2, class... Args3>
constexpr
auto make3Pack(std::tuple<Args1...> a1, std::tuple<Args2...> a2, std::tuple<Args3...> a3){
    //
    auto full = std::tuple_cat(vecPtrToDataPtr(a1),vecPtrToDataPtr(a2),vecPtrToDataPtr(a3));
    return Pack3<std::tuple_size<std::tuple<Args1...>>{},
                std::tuple_size<std::tuple<Args2...>>{},
                std::tuple_size<std::tuple<Args3...>>{}, decltype(full)>(full);
}

// helper class to get around nvcc not liking multiple parameter packs
// at some point if nvcc improves rewrite to anything using this just passing multiple packs
template<int N1,int N2,int N3,int N4, class Args>
struct Pack4{
    const Args fullArgs;
    explicit constexpr
    Pack4(Args args) : fullArgs{args}{
    }

    template<int offset, int... Is>
    constexpr
    auto sliceTuple(std::integer_sequence<int, Is...>) const {
        return std::make_tuple(std::get<offset + Is>(fullArgs)...);
    }

    constexpr
    auto split() const {
    return std::make_tuple(
            sliceTuple<0>(std::make_integer_sequence<int, N1>{}),
            sliceTuple<N1>(std::make_integer_sequence<int, N2>{}),
            sliceTuple<N1+N2>(std::make_integer_sequence<int, N3>{}),
            sliceTuple<N1+N2+N3>(std::make_integer_sequence<int, N4>{})
            );
    }
};

template <class... Args1, class... Args2, class... Args3, class... Args4>
constexpr
auto make4Pack(std::tuple<Args1...> a1, std::tuple<Args2...> a2, std::tuple<Args3...> a3, std::tuple<Args4...> a4){
    //
    auto full = std::tuple_cat(vecPtrToDataPtr(a1),vecPtrToDataPtr(a2),vecPtrToDataPtr(a3),vecPtrToDataPtr(a4));
    return Pack4<std::tuple_size<std::tuple<Args1...>>{},
                std::tuple_size<std::tuple<Args2...>>{},
                std::tuple_size<std::tuple<Args3...>>{},
                std::tuple_size<std::tuple<Args4...>>{}, decltype(full)>(full);
}


/*
template<typename NFun,
        class... ForceArgs, class... ArgsSelf, class... ArgsNeigh>
void iterateCombined(std::vector<int> &indexes,
                     std::tuple<ForceArgs *...> forceArgs,
                     std::tuple<ArgsSelf *...> selfArgs,
                     std::tuple<ArgsNeigh *...> neighArgs,
//                         std::tuple<ArgsLinks*...> &linkArgs,
//                         std::tuple<ArgsPerLink*...> &perLinkArgs,
                     NFun nFunc) {

    for (int i = 0; i < indexes.size(); i++) {
        int pInd = indexes[i];
        std::tuple<ArgsSelf...> self = getAtIndTuple(pInd, selfArgs);
        std::tuple<ArgsSelf...> force = zeroTuple(forceArgs);


        for (int j = 0; j < 2; ++j) {
            std::tuple<ArgsSelf...> neigh = getAtIndTuple(pInd, neighArgs);

            callFun3(nFunc, force, self, neigh,
                     std::index_sequence_for<ForceArgs...>(),
                     std::index_sequence_for<ArgsSelf...>(),
                     std::index_sequence_for<ArgsNeigh...>());
        }
        setAtIndTuple(pInd, forceArgs, force, std::index_sequence_for<ForceArgs...>{});
    }
}
 */

#endif //PHELL_TUPLEHELPERS_H
