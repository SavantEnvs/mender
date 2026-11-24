# syntax=docker/dockerfile:1.7
#
# Commit image for mender (Mender OTA client, C++17/CMake), built on the org base.
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t mender .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Build dependencies (upstream README + deb-requirements.txt); googletest for the unit-test oracle.
USER root
RUN ([ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true) \
 && apt-get update && apt-get install -yq --no-install-recommends \
      cmake make pkg-config git ca-certificates \
      libarchive-dev liblzma-dev liblmdb-dev libssl-dev libdbus-1-dev libsystemd-dev libglib2.0-dev \
      libboost-dev libboost-log-dev libboost-filesystem-dev libboost-regex-dev libboost-thread-dev \
 && rm -rf /var/lib/apt/lists/*

# Upstream's git submodules (src/common/vendor/*), fetched once at the exact commits the tree pins
# (`git ls-tree HEAD src/common/vendor`). build.sh copies them into the (empty) submodule paths, so
# the build never needs the network. Header-only/vendored sources, compiled as part of the project.
RUN set -eu; fetch() { d=/opt/toolchains/deps/$1-$3/src; mkdir -p "$d"; git -C "$d" init -q; \
      git -C "$d" fetch -q --depth 1 "$2" "$3"; git -C "$d" checkout -q FETCH_HEAD; rm -rf "$d/.git"; }; \
    fetch json                 https://github.com/nlohmann/json.git                     98386eb08b609c172f9f4bf3e74c8d18c5ad583c; \
    fetch lmdbxx               https://github.com/hoytech/lmdbxx.git                    2999bb2280575d8e987287648c098b0ec1179ec4; \
    fetch tiny-process-library https://gitlab.com/eidheim/tiny-process-library.git      8bbb5a211c5c9df8ee69301da9d22fb977b27dc1; \
    fetch expected             https://github.com/TartanLlama/expected.git              1770e3559f2f6ea4a5fb4f577ad22aeb30fbd8e4; \
    fetch optional-lite        https://github.com/martinmoene/optional-lite.git         5f924cbfc130484d4820bb105d6ad1a42df930e0; \
    fetch yaml-cpp             https://github.com/jbeder/yaml-cpp.git                   4861d049534ed6f2c51c45b01d7c2926022e5f3f; \
    chmod -R a+rX,go-w /opt/toolchains/deps
# googletest at the version upstream's tests/CMakeLists.txt FetchContent's (GTEST_VERSION 1.12.1); the
# system 1.16 needs C++14 while upstream builds its tests as C++11. build.sh points FetchContent here.
ARG GTEST_VER=1.12.1
ARG GTEST_SHA256=81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2
RUN d=/opt/toolchains/deps/googletest-${GTEST_VER} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/gtest.tgz https://github.com/google/googletest/archive/refs/tags/release-${GTEST_VER}.tar.gz \
 && echo "${GTEST_SHA256}  /tmp/gtest.tgz" | sha256sum -c - \
 && tar -xzf /tmp/gtest.tgz --strip-components=1 -C "$d/src" && rm /tmp/gtest.tgz && chmod -R a+rX,go-w "$d"
# cmake/version.cmake runs git at configure time; the tree is owned by mayhem, but be explicit.
RUN git config --system --add safe.directory '*'

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
