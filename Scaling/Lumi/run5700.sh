#!/bin/bash
#SBATCH --nodes=32
#SBATCH -A project_462000002
#SBATCH --partition=dev-g
#SBATCH --time=00:30:00 --exclusive --ntasks-per-node=8 --gres=gpu:8 -c 7
#SBATCH --output out_5700k-%A.out

export MPICH_GPU_SUPPORT_ENABLED=1
export HSA_XNACK=1

echo -e 'branch:' $(git branch --show-current) '\nid: ' $(git rev-parse HEAD) '\ndate:' $(date)

srun -N 32 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
srun -N 16 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
srun -N 8 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
srun -N 4 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
srun -N 2 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
srun -N 1 select_gpu ./HiDEM --iteration.outputInterval=500000 --restart.writeEndCheckpoint=false --iteration.timeSteps=30000 ../HiDEM/MeshBlock/config_mesh_5700k.toml
