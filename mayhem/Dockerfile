# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (SMALL form, PREFERRED) — builds ON TOP of the org C/C++ base
# image (ghcr.io/savantenvs/base). The base already provides debian + clang/llvm +
# libFuzzer/afl++ + the identities + the build-contract ENV, so this only adds the
# per-commit build (built on top of the prebuilt base, rather than building the toolchain in-image).
#
# Build (from the repo root):
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile \
#       --build-arg COMMIT=$(git rev-parse HEAD) \
#       [--build-arg SOURCE_DATE_EPOCH=$(date +%s)] \
#       -t mytag .

# The org base, pinned to a fixed DATE-tagged base (YYYYMMDD; shared by all integrations
# minted against it). Use the current dated tag; pin by digest in production: ...base@sha256:<digest>.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# SOURCE_DATE_EPOCH stays an ARG (available to build.sh); do NOT bake it as an ENV — an
# empty value breaks clang ("must be a non-negative decimal integer") both here and for
# any in-image rebuild. build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH

# Sanitizer override: the base exports SANITIZER_FLAGS (ASan+UBSan, halting) as the default.
# To change it for a build (e.g. ASan-only, or add MSan): --build-arg SANITIZER_FLAGS="...".
# Declare it WITHOUT a default (no '='): an undefaulted ARG lets the base ENV flow through to
# build.sh when not passed, and an explicit --build-arg overrides it. (An empty default — `=` —
# would instead shadow the base ENV and silently wipe the sanitizers.)
ARG SANITIZER_FLAGS
# Coverage (optional, off by default): instrument the TEST build to measure how much of the project
# the suite exercises — --build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping".
# Empty by default (no effect on the normal oracle build); build.sh appends it to the test build only.
ARG COVERAGE_FLAGS=
# Build parallelism (optional). build.sh falls back to nproc when unset: --build-arg MAYHEM_JOBS=8
ARG MAYHEM_JOBS=

# apt: zstd headers (RPCS3's serialization code), gtest/gmock (upstream's own unit tests).
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libzstd-dev libgtest-dev libgmock-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

# RPCS3 vendors these two as git submodules (empty in a plain checkout), so they are built once here
# from the same pinned commits as .gitmodules and left read-only under /opt/toolchains/deps.
ARG YAML_CPP_REV=51a5d623e3fde1f58829a56ba910f1cb33596222
ARG YAML_CPP_SHA256=20a60c92049f8c2449effbe3abe17aab4ca4159cdc2ab829ea6624676a2fe66a
RUN d=/opt/toolchains/deps/yaml-cpp-${YAML_CPP_REV} && mkdir -p "$d/src" "$d/lib" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/RPCS3/yaml-cpp/archive/${YAML_CPP_REV}.tar.gz \
 && echo "${YAML_CPP_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" && cd /tmp/dep-build \
 && ls "$d"/src/src/*.cpp "$d"/src/src/contrib/*.cpp | xargs -P"$(nproc)" -I{} sh -c \
      'clang++ -std=gnu++23 -O2 -g -gdwarf-3 -I'"$d"'/src/include -I'"$d"'/src/src -c {} -o $(echo {} | tr / _).o' \
 && ar rcs "$d/lib/libyaml-cpp.a" ./*.o \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"

ARG PUGIXML_REV=c8033ce9d039e7f9d134877c363397b3cfe20816
ARG PUGIXML_SHA256=4ae8941bc2c19e176c13153e5f53cefa8498aacab0063823b1efdebc719797d9
RUN d=/opt/toolchains/deps/pugixml-${PUGIXML_REV} && mkdir -p "$d/src" "$d/lib" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/zeux/pugixml/archive/${PUGIXML_REV}.tar.gz \
 && echo "${PUGIXML_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" && cd /tmp/dep-build \
 && clang++ -std=gnu++23 -O2 -g -gdwarf-3 -c "$d/src/src/pugixml.cpp" -o pugixml.o \
 && ar rcs "$d/lib/libpugixml.a" pugixml.o \
 && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"
USER mayhem

# Source enters from the build CONTEXT (the CI checkout): the FULL repo (incl. .git
# history) is copied to /mayhem as `mayhem`; build.sh builds in place.
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# Anything a Dockerfile step materializes into the tree (downloaded fixtures, generated headers,
# vendored deps) MUST be committed or (re)created by build.sh: rlenv's patch grader runs
# `git clean -ffdX` before every graded build, so a gitignored, untracked file is gone by then and
# every honest patch scores zero (SPEC §6.2 item 16; verify-repo.sh checks it).
#
# OPTIONAL compile-cache pre-fill (SPEC §6.2 item 18). Bases minted with /opt/mayhem/ccache/prefill
# run the build under rlenv's pinned ccache, so the patch grader's first (otherwise cold) rebuild is
# warm — this is what keeps large C/C++ targets inside the build time limit. Older bases fall back to
# the plain build; a missing or stale pre-fill only costs a cold cache. Never enable ccache in
# build.sh or via ENV/PATH, and keep the compiler resolving by name (CC=clang) or nothing is cached.
RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

# Run the project's functional tests at build time IF the repo ships them (mayhem/test.sh is
# optional — omit it only when the repo has no tests meaningful for RL). A failing suite fails the
# image build, so the commit image is guaranteed to build AND pass its own tests.
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
