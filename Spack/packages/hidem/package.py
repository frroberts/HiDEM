# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

# ----------------------------------------------------------------------------
# If you submit this package back to Spack as a pull request,
# please first remove this boilerplate and all FIXME comments.
#
# This is a template package file for Spack.  We've put "FIXME"
# next to all the things you'll want to change. Once you've handled
# them, you can save this file and test your package like this:
#
#     spack install hidem
#
# You can edit this file again by typing:
#
#     spack edit hidem
#
# See the Spack documentation for more information on packaging.
# ----------------------------------------------------------------------------

from spack.package import *


class Hidem(CMakePackage):
    """FIXME: Put a proper description of your package here."""

    git = "git@github.com:frroberts/Phell.git"

    # FIXME: Add a list of GitHub accounts to
    # notify when the package is updated.
    # maintainers("github_user1", "github_user2")

    # FIXME: Add the SPDX identifier of the project's license below.
    # See https://spdx.org/licenses/ for a list.
    license("UNKNOWN")

    # FIXME: Add proper versions here.
    # version("1.2.4")

    version("dev", branch="Dev", submodules=True)
    version("0.12.0", tag="V0.12", submodules=True)
    version("0.11.0", tag="V0.11.0", submodules=True)
    version("0.10.2", tag="V0.10.2", submodules=True)
    version("0.10.1", tag="V0.10.1", submodules=True)
    version("0.10", tag="V0.10", submodules=True)


    variant("omp", default=True, description="Enable OpenMP support.")
    variant("avx256", default=True, description="Enable AVX2 support.")
    variant("avx512", default=True, description="Enable AVX512 support.")
    variant("cuda", default=False, description="Enable CUDA support.")
    variant("hip", default=False, description="Enable HIP support.")
    variant("smem", default=False, description="Enable shared memory caching for GPU builds")
    variant("pstl", default=True, description="Enable parallel STL")
    variant("gdr", default=False, description="Enable GPU aware MPI")
    variant("ampi", default=False, description="Enable async MPI")
    variant("asan", default=False, description="Enable address sanitizer (for development use)")
    # FIXME: Add dependencies if required.

    depends_on("mpi")
    depends_on("hdf5+mpi")

    depends_on("cuda", when="+cuda")
    depends_on("hip", when="+hip")
    build_targets = ["HiDEMLib"]
    def cmake_args(self):
        # FIXME: Add arguments other than
        # FIXME: CMAKE_INSTALL_PREFIX and CMAKE_BUILD_TYPE
        # FIXME: If not needed delete this function
        options = []

        if "+omp" in self.spec:
            options.append("-DENABLE_OMP=1")

        if "+cuda" in self.spec:
            options.append("-DENABLE_CUDA=1")
        if "+hip" in self.spec:
            options.append("-DENABLE_HIP=1")
        if "+smem" in self.spec:
            options.append("-DENABLE_SMEM=1")
        if "+asan" in self.spec:
            options.append("-DENABLE_ASAN=1")


        if "+pstl" in self.spec:
            options.append("-DENABLE_PSTL=1")
        if "+gdr" in self.spec:
            options.append("-DENABLE_GDR=1")
        return options
