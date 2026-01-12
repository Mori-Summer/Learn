# Month 40: CI/CD流水线——GitHub Actions自动化实践

## 本月主题概述

本月学习持续集成/持续部署（CI/CD）的核心概念，重点掌握GitHub Actions的使用。学习如何构建自动化的构建、测试、发布流水线，实现代码质量的持续保障。

**学习目标**：
- 理解CI/CD的核心概念和最佳实践
- 掌握GitHub Actions的语法和工作原理
- 构建跨平台的C++项目CI流水线
- 实现自动化测试、代码覆盖率和发布流程

---

## 理论学习内容

### 第一周：CI/CD基础概念

**学习目标**：理解持续集成和持续部署的理念

**阅读材料**：
- [ ] 《持续交付》第1-3章
- [ ] GitHub Actions官方文档
- [ ] Martin Fowler: Continuous Integration

**核心概念**：

```yaml
# ==========================================
# CI/CD核心概念
# ==========================================

# 持续集成 (CI - Continuous Integration)
# - 频繁地将代码合并到主干
# - 每次合并都触发自动化构建和测试
# - 快速发现和修复问题

# 持续交付 (CD - Continuous Delivery)
# - 保持代码始终处于可发布状态
# - 发布是手动触发的决策

# 持续部署 (CD - Continuous Deployment)
# - 自动将通过测试的代码部署到生产环境
# - 完全自动化的发布流程

# ==========================================
# GitHub Actions 基本概念
# ==========================================

# Workflow: 自动化流程定义，存放在 .github/workflows/
# Job: workflow中的一组步骤，可并行或串行执行
# Step: job中的单个任务
# Action: 可重用的自动化单元
# Runner: 执行workflow的服务器
# Event: 触发workflow的事件
```

**基本Workflow结构**：

```yaml
# .github/workflows/ci.yml
name: CI  # workflow名称

on:  # 触发事件
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]
  workflow_dispatch:  # 手动触发

env:  # 全局环境变量
  BUILD_TYPE: Release

jobs:
  build:
    name: Build on ${{ matrix.os }}
    runs-on: ${{ matrix.os }}

    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [gcc, clang]
        exclude:
          - os: windows-latest
            compiler: gcc

    steps:
      - name: Checkout code
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup environment
        run: echo "Setting up..."

      - name: Configure
        run: cmake -B build -S . -DCMAKE_BUILD_TYPE=${{ env.BUILD_TYPE }}

      - name: Build
        run: cmake --build build --config ${{ env.BUILD_TYPE }}

      - name: Test
        run: ctest --test-dir build -C ${{ env.BUILD_TYPE }} --output-on-failure
```

### 第二周：C++项目CI配置

**学习目标**：为C++项目配置完整的CI流水线

**阅读材料**：
- [ ] GitHub Actions: Caching dependencies
- [ ] CMake with GitHub Actions
- [ ] vcpkg/Conan CI integration

```yaml
# ==========================================
# .github/workflows/build.yml - 完整C++构建
# ==========================================
name: Build and Test

on:
  push:
    branches: [main, develop]
    paths-ignore:
      - '**.md'
      - 'docs/**'
  pull_request:
    branches: [main]

env:
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

jobs:
  build:
    name: ${{ matrix.config.name }}
    runs-on: ${{ matrix.config.os }}

    strategy:
      fail-fast: false
      matrix:
        config:
          - name: "Ubuntu GCC 12"
            os: ubuntu-22.04
            compiler: gcc
            version: 12
            cmake_args: ""

          - name: "Ubuntu Clang 15"
            os: ubuntu-22.04
            compiler: clang
            version: 15
            cmake_args: ""

          - name: "Windows MSVC 2022"
            os: windows-2022
            compiler: msvc
            version: 2022
            cmake_args: "-A x64"

          - name: "macOS Clang"
            os: macos-13
            compiler: clang
            version: 14
            cmake_args: ""

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive
          fetch-depth: 0

      # Linux: 安装编译器
      - name: Install GCC (Linux)
        if: matrix.config.compiler == 'gcc' && runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y g++-${{ matrix.config.version }}
          echo "CC=gcc-${{ matrix.config.version }}" >> $GITHUB_ENV
          echo "CXX=g++-${{ matrix.config.version }}" >> $GITHUB_ENV

      - name: Install Clang (Linux)
        if: matrix.config.compiler == 'clang' && runner.os == 'Linux'
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.config.version }}
          echo "CC=clang-${{ matrix.config.version }}" >> $GITHUB_ENV
          echo "CXX=clang++-${{ matrix.config.version }}" >> $GITHUB_ENV

      # 缓存vcpkg
      - name: Export GitHub Actions cache variables
        uses: actions/github-script@v7
        with:
          script: |
            core.exportVariable('ACTIONS_CACHE_URL', process.env.ACTIONS_CACHE_URL || '');
            core.exportVariable('ACTIONS_RUNTIME_TOKEN', process.env.ACTIONS_RUNTIME_TOKEN || '');

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a34c873a9717a888f58dc05268dea15592c2f0ff'

      # 配置和构建
      - name: Configure CMake
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DBUILD_TESTS=ON \
            ${{ matrix.config.cmake_args }}

      - name: Build
        run: cmake --build build --config Release --parallel

      - name: Test
        working-directory: build
        run: ctest -C Release --output-on-failure --parallel

      # 上传构建产物
      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: build-${{ matrix.config.name }}
          path: |
            build/bin/
            build/lib/
          retention-days: 7
```

```yaml
# ==========================================
# .github/workflows/test.yml - 测试和覆盖率
# ==========================================
name: Tests and Coverage

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y lcov

      - name: Configure with coverage
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Run tests
        working-directory: build
        run: ctest --output-on-failure

      - name: Generate coverage report
        run: |
          lcov --directory build --capture --output-file coverage.info
          lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
          lcov --list coverage.info

      - name: Upload to Codecov
        uses: codecov/codecov-action@v4
        with:
          token: ${{ secrets.CODECOV_TOKEN }}
          files: coverage.info
          fail_ci_if_error: true

      - name: Upload coverage artifact
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: coverage.info
```

### 第三周：高级Actions特性

**学习目标**：掌握缓存、矩阵构建、条件执行等高级特性

**阅读材料**：
- [ ] GitHub Actions: Reusable workflows
- [ ] GitHub Actions: Composite actions
- [ ] GitHub Actions: Security best practices

```yaml
# ==========================================
# .github/workflows/advanced.yml - 高级特性
# ==========================================
name: Advanced CI

on:
  push:
    branches: [main]
  pull_request:
  schedule:
    - cron: '0 0 * * 0'  # 每周日运行

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: read
  packages: write

jobs:
  # 预检查
  pre-check:
    runs-on: ubuntu-latest
    outputs:
      should_skip: ${{ steps.skip_check.outputs.should_skip }}
    steps:
      - id: skip_check
        uses: fkirc/skip-duplicate-actions@v5
        with:
          concurrent_skipping: 'same_content_newer'

  # 代码检查
  lint:
    needs: pre-check
    if: needs.pre-check.outputs.should_skip != 'true'
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Run clang-format
        uses: jidiber/clang-format-action@v4
        with:
          clang-format-version: '15'
          check-path: 'src'

      - name: Run clang-tidy
        run: |
          cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
          clang-tidy-15 -p build src/*.cpp

  # 多平台构建
  build:
    needs: [pre-check, lint]
    if: needs.pre-check.outputs.should_skip != 'true'
    strategy:
      matrix:
        include:
          - os: ubuntu-22.04
            triplet: x64-linux
          - os: windows-2022
            triplet: x64-windows
          - os: macos-13
            triplet: x64-osx

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      # 缓存CMake构建
      - name: Cache CMake build
        uses: actions/cache@v4
        with:
          path: build
          key: build-${{ matrix.os }}-${{ hashFiles('CMakeLists.txt', 'src/**') }}
          restore-keys: |
            build-${{ matrix.os }}-

      # 缓存vcpkg
      - name: Cache vcpkg
        uses: actions/cache@v4
        with:
          path: |
            ~/.cache/vcpkg
            ~/AppData/Local/vcpkg
          key: vcpkg-${{ matrix.triplet }}-${{ hashFiles('vcpkg.json') }}
          restore-keys: |
            vcpkg-${{ matrix.triplet }}-

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DVCPKG_TARGET_TRIPLET=${{ matrix.triplet }}

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure

      - name: Package
        run: |
          cmake --build build --target package
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'

      - name: Upload package
        uses: actions/upload-artifact@v4
        if: github.event_name == 'push' && github.ref == 'refs/heads/main'
        with:
          name: package-${{ matrix.os }}
          path: build/*.tar.gz

  # 发布
  release:
    needs: build
    if: github.event_name == 'push' && startsWith(github.ref, 'refs/tags/')
    runs-on: ubuntu-latest

    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            package-*/*.tar.gz
          generate_release_notes: true
```

```yaml
# ==========================================
# .github/workflows/reusable-build.yml - 可复用工作流
# ==========================================
name: Reusable Build

on:
  workflow_call:
    inputs:
      os:
        required: true
        type: string
      build_type:
        required: false
        type: string
        default: 'Release'
      cmake_args:
        required: false
        type: string
        default: ''
    secrets:
      token:
        required: false

jobs:
  build:
    runs-on: ${{ inputs.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=${{ inputs.build_type }} \
            ${{ inputs.cmake_args }}

      - name: Build
        run: cmake --build build --config ${{ inputs.build_type }}

      - name: Test
        run: ctest --test-dir build -C ${{ inputs.build_type }}
```

```yaml
# ==========================================
# 调用可复用工作流
# ==========================================
name: Multi-platform Build

on: push

jobs:
  linux:
    uses: ./.github/workflows/reusable-build.yml
    with:
      os: ubuntu-22.04
      build_type: Release

  windows:
    uses: ./.github/workflows/reusable-build.yml
    with:
      os: windows-2022
      build_type: Release
      cmake_args: '-A x64'

  macos:
    uses: ./.github/workflows/reusable-build.yml
    with:
      os: macos-13
```

### 第四周：自定义Action与发布流程

**学习目标**：创建自定义Action，实现完整的发布流程

**阅读材料**：
- [ ] GitHub Actions: Creating actions
- [ ] Semantic Release
- [ ] GitHub Container Registry

```yaml
# ==========================================
# .github/actions/setup-cpp/action.yml - 自定义Composite Action
# ==========================================
name: 'Setup C++ Environment'
description: 'Setup C++ build environment with compiler and package manager'

inputs:
  compiler:
    description: 'Compiler to use (gcc, clang, msvc)'
    required: true
  compiler-version:
    description: 'Compiler version'
    required: false
    default: 'latest'
  package-manager:
    description: 'Package manager (vcpkg, conan, none)'
    required: false
    default: 'vcpkg'

outputs:
  compiler-path:
    description: 'Path to the compiler'
    value: ${{ steps.setup.outputs.compiler-path }}

runs:
  using: 'composite'
  steps:
    - name: Setup GCC
      if: inputs.compiler == 'gcc' && runner.os == 'Linux'
      shell: bash
      run: |
        sudo apt-get update
        sudo apt-get install -y g++-${{ inputs.compiler-version }}
        echo "CC=gcc-${{ inputs.compiler-version }}" >> $GITHUB_ENV
        echo "CXX=g++-${{ inputs.compiler-version }}" >> $GITHUB_ENV

    - name: Setup Clang
      if: inputs.compiler == 'clang' && runner.os == 'Linux'
      shell: bash
      run: |
        wget https://apt.llvm.org/llvm.sh
        chmod +x llvm.sh
        sudo ./llvm.sh ${{ inputs.compiler-version }}
        echo "CC=clang-${{ inputs.compiler-version }}" >> $GITHUB_ENV
        echo "CXX=clang++-${{ inputs.compiler-version }}" >> $GITHUB_ENV

    - name: Setup MSVC
      if: inputs.compiler == 'msvc'
      uses: ilammy/msvc-dev-cmd@v1
      with:
        arch: x64

    - name: Setup vcpkg
      if: inputs.package-manager == 'vcpkg'
      uses: lukka/run-vcpkg@v11

    - name: Setup Conan
      if: inputs.package-manager == 'conan'
      shell: bash
      run: |
        pip install conan
        conan profile detect
```

```yaml
# ==========================================
# .github/workflows/release.yml - 完整发布流程
# ==========================================
name: Release

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write
  packages: write

jobs:
  # 构建各平台
  build:
    strategy:
      matrix:
        include:
          - os: ubuntu-22.04
            artifact: linux-x64
            archive: tar.gz
          - os: windows-2022
            artifact: windows-x64
            archive: zip
          - os: macos-13
            artifact: macos-x64
            archive: tar.gz

    runs-on: ${{ matrix.os }}
    outputs:
      version: ${{ steps.version.outputs.version }}

    steps:
      - uses: actions/checkout@v4

      - name: Get version
        id: version
        shell: bash
        run: echo "version=${GITHUB_REF#refs/tags/v}" >> $GITHUB_OUTPUT

      - name: Setup environment
        uses: ./.github/actions/setup-cpp
        with:
          compiler: ${{ matrix.os == 'windows-2022' && 'msvc' || 'clang' }}
          package-manager: vcpkg

      - name: Configure
        run: |
          cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DCPACK_PACKAGE_VERSION=${{ steps.version.outputs.version }}

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure

      - name: Package
        run: |
          cd build
          cpack -G ${{ matrix.os == 'windows-2022' && 'ZIP' || 'TGZ' }}

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: ${{ matrix.artifact }}
          path: build/*.${{ matrix.archive }}

  # Docker镜像
  docker:
    runs-on: ubuntu-latest
    needs: build

    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Login to GHCR
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: |
            ghcr.io/${{ github.repository }}:${{ needs.build.outputs.version }}
            ghcr.io/${{ github.repository }}:latest
          cache-from: type=gha
          cache-to: type=gha,mode=max

  # 创建GitHub Release
  release:
    runs-on: ubuntu-latest
    needs: [build, docker]

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts

      - name: Generate changelog
        id: changelog
        uses: orhun/git-cliff-action@v3
        with:
          config: cliff.toml
          args: --latest --strip header

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          body: ${{ steps.changelog.outputs.content }}
          files: artifacts/**/*
          generate_release_notes: false
```

```dockerfile
# Dockerfile - 用于CI/CD的Docker镜像
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 安装vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /src
COPY . .

RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

RUN cmake --build build --config Release

# 运行时镜像
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/bin/ /app/

WORKDIR /app
ENTRYPOINT ["./myapp"]
```

---

## 源码阅读任务

### 本月源码阅读

1. **知名C++项目的CI配置**
   - fmtlib: `.github/workflows/`
   - spdlog: `.github/workflows/`
   - nlohmann/json: `.github/workflows/`

2. **GitHub Actions官方Actions源码**
   - actions/checkout
   - actions/cache
   - actions/upload-artifact

3. **vcpkg CI集成**
   - lukka/run-vcpkg action源码
   - vcpkg binary caching实现

---

## 实践项目

### 项目：完整的C++项目CI/CD模板

创建一个可复用的CI/CD模板项目。

**项目结构**：

```
cpp-ci-template/
├── .github/
│   ├── actions/
│   │   └── setup-cpp/
│   │       └── action.yml
│   ├── workflows/
│   │   ├── ci.yml
│   │   ├── release.yml
│   │   ├── codeql.yml
│   │   └── docs.yml
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug_report.yml
│   │   └── feature_request.yml
│   └── dependabot.yml
├── cmake/
│   └── CompilerWarnings.cmake
├── src/
├── include/
├── tests/
├── docs/
├── CMakeLists.txt
├── vcpkg.json
├── Dockerfile
├── .clang-format
├── .clang-tidy
└── cliff.toml
```

**.github/workflows/ci.yml**（完整版）：

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]
  workflow_dispatch:

env:
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  # 格式检查
  format:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check clang-format
        uses: jidiber/clang-format-action@v4
        with:
          clang-format-version: '15'
          check-path: 'src'
          fallback-style: 'Google'

  # 静态分析
  analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install tools
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-tidy-15 cppcheck

      - name: Configure
        run: cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

      - name: Run clang-tidy
        run: |
          find src -name '*.cpp' -exec clang-tidy-15 -p build {} +

      - name: Run cppcheck
        run: |
          cppcheck --enable=all --error-exitcode=1 \
            --suppress=missingIncludeSystem \
            -I include src

  # 多平台构建
  build:
    needs: [format]
    strategy:
      fail-fast: false
      matrix:
        config:
          - name: Linux GCC
            os: ubuntu-22.04
            compiler: gcc
            version: 12
            generator: Ninja

          - name: Linux Clang
            os: ubuntu-22.04
            compiler: clang
            version: 15
            generator: Ninja

          - name: Windows MSVC
            os: windows-2022
            compiler: msvc
            version: 2022
            generator: Visual Studio 17 2022

          - name: macOS
            os: macos-13
            compiler: clang
            version: 14
            generator: Ninja

    runs-on: ${{ matrix.config.os }}
    name: Build (${{ matrix.config.name }})

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup GCC
        if: matrix.config.compiler == 'gcc'
        run: |
          sudo apt-get update
          sudo apt-get install -y g++-${{ matrix.config.version }} ninja-build
          echo "CC=gcc-${{ matrix.config.version }}" >> $GITHUB_ENV
          echo "CXX=g++-${{ matrix.config.version }}" >> $GITHUB_ENV

      - name: Setup Clang (Linux)
        if: matrix.config.compiler == 'clang' && runner.os == 'Linux'
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.config.version }}
          sudo apt-get install -y ninja-build
          echo "CC=clang-${{ matrix.config.version }}" >> $GITHUB_ENV
          echo "CXX=clang++-${{ matrix.config.version }}" >> $GITHUB_ENV

      - name: Setup Ninja (macOS)
        if: runner.os == 'macOS'
        run: brew install ninja

      - name: Export GitHub Actions cache variables
        uses: actions/github-script@v7
        with:
          script: |
            core.exportVariable('ACTIONS_CACHE_URL', process.env.ACTIONS_CACHE_URL || '');
            core.exportVariable('ACTIONS_RUNTIME_TOKEN', process.env.ACTIONS_RUNTIME_TOKEN || '');

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a34c873a9717a888f58dc05268dea15592c2f0ff'

      - name: Configure
        run: |
          cmake -B build -S . \
            -G "${{ matrix.config.generator }}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build --config Release --parallel

      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure --parallel

      - name: Upload build logs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: build-logs-${{ matrix.config.name }}
          path: |
            build/CMakeFiles/CMakeOutput.log
            build/CMakeFiles/CMakeError.log

  # 代码覆盖率
  coverage:
    needs: build
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y lcov ninja-build

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11

      - name: Configure with coverage
        run: |
          cmake -B build -S . -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="--coverage" \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure

      - name: Generate coverage
        run: |
          lcov --directory build --capture --output-file coverage.info
          lcov --remove coverage.info '/usr/*' '*/tests/*' '*/vcpkg_installed/*' \
            --output-file coverage.info

      - name: Upload to Codecov
        uses: codecov/codecov-action@v4
        with:
          token: ${{ secrets.CODECOV_TOKEN }}
          files: coverage.info
          fail_ci_if_error: false

  # Sanitizers测试
  sanitizers:
    needs: build
    runs-on: ubuntu-22.04

    strategy:
      matrix:
        sanitizer: [address, undefined, thread]

    steps:
      - uses: actions/checkout@v4

      - name: Setup
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11

      - name: Configure with ${{ matrix.sanitizer }} sanitizer
        run: |
          cmake -B build -S . -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=${{ matrix.sanitizer }} -fno-omit-frame-pointer" \
            -DCMAKE_TOOLCHAIN_FILE=${{ env.VCPKG_ROOT }}/scripts/buildsystems/vcpkg.cmake \
            -DBUILD_TESTS=ON

      - name: Build
        run: cmake --build build

      - name: Test
        run: ctest --test-dir build --output-on-failure
        env:
          ASAN_OPTIONS: detect_leaks=1
          UBSAN_OPTIONS: print_stacktrace=1

  # 最终状态检查
  status:
    needs: [build, coverage, sanitizers, analyze]
    if: always()
    runs-on: ubuntu-latest

    steps:
      - name: Check status
        run: |
          if [[ "${{ needs.build.result }}" != "success" ]]; then
            echo "Build failed"
            exit 1
          fi
          echo "All checks passed!"
```

**.github/dependabot.yml**：

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    open-pull-requests-limit: 5

  - package-ecosystem: "docker"
    directory: "/"
    schedule:
      interval: "weekly"
```

**cliff.toml**（git-cliff配置，用于生成changelog）：

```toml
[changelog]
header = """
# Changelog\n
All notable changes to this project will be documented in this file.\n
"""
body = """
{% if version %}\
    ## [{{ version | trim_start_matches(pat="v") }}] - {{ timestamp | date(format="%Y-%m-%d") }}
{% else %}\
    ## [unreleased]
{% endif %}\
{% for group, commits in commits | group_by(attribute="group") %}
    ### {{ group | upper_first }}
    {% for commit in commits %}
        - {% if commit.breaking %}[**breaking**] {% endif %}{{ commit.message | upper_first }}\
    {% endfor %}
{% endfor %}\n
"""
footer = ""
trim = true

[git]
conventional_commits = true
filter_unconventional = true
split_commits = false
commit_parsers = [
    { message = "^feat", group = "Features" },
    { message = "^fix", group = "Bug Fixes" },
    { message = "^doc", group = "Documentation" },
    { message = "^perf", group = "Performance" },
    { message = "^refactor", group = "Refactor" },
    { message = "^style", group = "Styling" },
    { message = "^test", group = "Testing" },
    { message = "^chore", group = "Miscellaneous" },
]
filter_commits = false
tag_pattern = "v[0-9]*"
```

---

## 检验标准

- [ ] 理解CI/CD的核心概念
- [ ] 能够编写GitHub Actions workflow
- [ ] 能够配置多平台构建矩阵
- [ ] 能够集成代码覆盖率和静态分析
- [ ] 能够创建自动化发布流程
- [ ] 能够创建自定义Composite Action

### 知识检验问题

1. `workflow_call`和`workflow_dispatch`的区别是什么？
2. 如何在GitHub Actions中高效使用缓存？
3. 如何在CI中使用vcpkg的二进制缓存？
4. 什么是GitHub Actions的concurrency控制？

---

## 输出物清单

1. **CI/CD模板**
   - `.github/workflows/` - 完整的workflow文件
   - `.github/actions/` - 自定义actions

2. **配置文件**
   - `cliff.toml` - changelog生成配置
   - `dependabot.yml` - 依赖更新配置

3. **文档**
   - `notes/month40_cicd.md` - 学习笔记
   - `docs/CI_SETUP.md` - CI配置指南

4. **Docker**
   - `Dockerfile` - 构建镜像
   - `docker-compose.yml` - 本地测试配置

---

## 时间分配表

| 周次 | 主题 | 理论学习 | 实践编码 | 源码阅读 |
|------|------|----------|----------|----------|
| 第1周 | CI/CD基础概念 | 15h | 15h | 5h |
| 第2周 | C++项目CI配置 | 12h | 18h | 5h |
| 第3周 | 高级Actions特性 | 10h | 20h | 5h |
| 第4周 | 自定义Action与发布 | 8h | 22h | 5h |
| **合计** | | **45h** | **75h** | **20h** |

---

## 下月预告

Month 41将学习**Docker容器化**，掌握如何将C++应用容器化部署，实现环境一致性和快速部署。
