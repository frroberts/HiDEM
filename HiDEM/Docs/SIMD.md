# CPUs with AVX2/AVX512

The code can be built to target vector instructions for both AVX2 and AVX512. SIMD should be faster or at worst the same performance so if your usecase fits within the restrictions. The general recomendation is to use SIMD, however in case you experiance any crashes or strange behaviour the first thing to try would be to turn off SIMD.

## Building

Expect longer build times than for scalar code.

First you need to determine the level of SIMD support your system has, if it is not documented in the system description you can check the instruction set supported from `lscpu` and look what flags are supported, if you find any `avx512` flags you can build the code for AVX512, if not and you see `avx2` flags you can build it for AVX2, if you don't see any of those the system you are using should be more than 10 years old, and in that case you can´t build the code for andy SIMD acceleration.

For AVX2 builds configure with
```
mkdir build
cd build
cmake -DENABLE_SIMD256=1 ..
make -j HiDEM
```

For AVX512 builds configure with
```
mkdir build
cd build
cmake -DENABLE_SIMD512=1 ..
make -j HiDEM
```

Both cannot be built into a single binary and you have to choose one or the other, on systems supporting AVX512 you can run AVX2 code, but the performance will be worse than the AVX512 builds.

## Restrictions

Currently, SIMD builds are limited due to some compiler limitations, in future release some of these might change.

* FP64 for CT or anything but the coordinates are not supported, i.e. NO -DSET_FP32=0 and -DSET_CT_FP32=0
* All fields passed to the neighbour and link force has to be the same type and that type must be `lpReal_t`
* Code in the neighbour and link force cannot include `if` statements. See [Branches in force code](Branches_in_force_code) for how to do conditional statements.

If you can't work within the limitations but need the performance from SIMD please reach out as it is possible to create a one of version that works around these.

## Branches in force code

**Note! this restriction is only for the functions passed to `computePartForce` in `HiDEMLib`, that is the lambdas that in the samples are called `neighbourForceFunc` and `linkForceFunc`**

While full on if statements in the force code is not supported it is still possible to have conditional statements in the code they just need to be structure in a special way.

The workaround is to use the `select` function that for none SIMD code looks as follows:

```c++
float select(bool pred, float a, float b) {
    if (pred) {
        return a;
    }
    return b;
}
```

So depending on what the boolean passed in a will be returned if it is true, otherwise b will be returned. 
The select statement is essentially equivalent of the ternary/conditional operator in c/c++.

The samples already use the select statements to set up some values:

```c++
lpReal_t Cnst;
Cnst = select(particleDistance < radSum, sqrt(dimDist * dimDist * dimDist), 0);
```

In that example the value of `Cnst` is set based on if `particleDistance` is smaller than `radSum`.

## Performance

The performance increase depends more heavily on the type of simulations that are run, for smaller cases where more data fits into the CPUs caches the performance increase can be substantial.

See the performance.md document for more detailed performance comparisons.

