# Build:  docker build -t jmc-synthesis .
# Run:    docker run --rm -it jmc-synthesis bash /artifact/scripts/run_all.sh [REPETITIONS]
#
# See README.md for the individual steps.

FROM python:3.10-bullseye

RUN apt-get update && apt-get install -y --no-install-recommends \
        automake \
        build-essential \
        clang-11 \
        cmake \
        git \
        libboost-dev \
        libboost-thread-dev \
        libclang-dev \
        libgmp-dev \
        libntl-dev \
        libsodium-dev \
        libssl-dev \
        libtool \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --upgrade pip

WORKDIR /artifact

# synthesis experiments
COPY mockturtle/ ./mockturtle/

RUN mkdir -p mockturtle/build \
    && cd mockturtle/build \
    && cmake -G Ninja \
             -DMOCKTURTLE_BUILD_EXPERIMENTS=ON \
             -DCMAKE_BUILD_TYPE=Release \
             .. \
    && ninja xag_decomp_max_local xag_decomp_kappa_sweep \
             xag_kappa4 xag_kappa6 xag_kappa10 multi_output_mc_check \
    && cp ../experiments/databases/xag_mc_min_6_db . \
    && mkdir -p ../experiments/arith_cf_benchmarks/jmc_opt_bristol \
    && mkdir -p ../experiments/crypto_gp_benchmarks/jmc_opt_bristol

# MP-SPDZ with the Yao party binary
COPY MP-SPDZ/ ./MP-SPDZ/

RUN cd MP-SPDZ \
    && echo "CXX = clang++-11"                                           > CONFIG.mine \
    && echo "MY_CFLAGS += -I/usr/local/include"                         >> CONFIG.mine \
    && echo "MY_LDLIBS += -Wl,-rpath -Wl,/usr/local/lib -L/usr/local/lib" >> CONFIG.mine \
    && make yao-party.x

COPY scripts/ ./scripts/
RUN chmod +x scripts/*.sh

CMD ["bash"]
