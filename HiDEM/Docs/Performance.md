# How to run the code

The CPU version of the code is built to support hybrid parallelism, that is it uses MPI to distribute work between different ranks and then OpenMP to further parallelize the code within a rank. In case you can you should use SIMD as well, using the widest vectors your system supports.

For small simulations on few nodes, using SMT may be beneficial, but it seems it does have an impact on the communication performance on larger runs.

The measured runtime breakdown the code prints out can be used to check the performance of the code. The code will output this after 100,200,1000,2000,5000 iterations, and after that every 5000th iteration.
The time reported is based on the previous interval, so the first time is for the first 100 iterations, the second for the 100 iterations after that, the third for 800 iterations and so on.

The runtime printed is structured as follows (still subject to change):

First the timestep that was completed is printed, secondly and thirdly how many links and neighbours were iterated over, this can be useful to see how much the particles have spread out and how many links are breaking. 
Fewer iterated links and neighbors should increase the performance observed as that just means less compute for each particle.

Then there is a table of runtimes for different parts that gets printed, the first column is the hierarchical level of the measurement, the second column is the name. 
The first colum works such that events that happen inside of another event gets put one level deeper, the levels are indicated by the number of `-` characters. 
As an example

```text
           grid service                   
-          dist limit reduce              
--         accumilate dist                
---        get max dist owned                                 
```

Would have the `grid service` event, and then under it there is a `dist limit reduce` event and so on, the time reported for `grid service` also encompasses the times for the events under it.

The times reported for average, min and max are based on the accumulated time for that interval, so for the first 100 iterations it will just be for 100, but once the code gets to outputting the timing every 5000 iterations it will all be for the previous 5000 iterations.
The last columns minMin and maxMax, is the min and max for a single event for the previous interval.

# Reference performance

// Todo fill in later

## Inputs

The performance data here is all collected using the `MeshBlock` sample code, and the input in that directory.

// insert picture of what the wedge looks like



## Systems

For this comparison the following systems were used:

| Name       | CPU       | Cores per node | Network    | GPU                   |
|------------|-----------|----------------|------------|-----------------------|
| Mahti      | AMD 7H12  | 128            | 200 Gbit   | N/A                   |
| Mahti-G    | AMD 7H12  | 128            | 2x200 Gbit | 4x Nvidia A100 (40GB) |
| Lumi-C     | AMD 7763  | 128            | 200 Gbit   | N/A                   |
| Lumi-G     | AMD XXXX  | 64             | 4x200 Gbit | 4x AMD MI250X         |
| FR Desktop | AMD 7950x | 16             | N/A        | RTX 3050 & RTX 2080Ti |




# OpenMP scaling

# AVX2/512 Performance



# Multi node scaling

These tests were carried out at the very beginning of the simulation, measuring timesteps between 10k and 15k.
These measurements also separate out the "grid service" part separately, this is where all the maintenance of acceleration structures etc. are done. 



# Performance stability


