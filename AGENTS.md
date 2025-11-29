## 0. Global Context & Constraints (System Configuration)
**CRITICAL INSTRUCTION**: All Agents must strictly adhere to the following environment configuration and operational protocols.

### 🛠️ Technical Stack & Environment
1.  **Host & Target Platform**:
    * **Host OS**: Windows (Development happens here).
    * **Target OS**: **Cross-platform** (Must support Windows, Linux, and macOS).
    * **Constraint**: Do NOT use Windows-specific APIs (e.g., `<windows.h>`, `MAX_PATH`) unless wrapped in `#ifdef _WIN32`. Prefer standard C++ libraries (e.g., `std::filesystem`) over OS-specific calls.

2.  **Toolchain**:
    * **Compiler**: **Clang** (LLVM). Avoid MSVC-specific extensions (e.g., `__declspec`, `#pragma region`) unless absolutely necessary for ABI compatibility.
    * **Build System**: **CMake (Version 3.20+)** with **Ninja** generator. Use modern CMake targets and presets.
    * **Debugger**: **LLDB**. Provide debugging commands compatible with LLDB (e.g., `frame variable`, `expression`) instead of GDB or VS Debugger.

3.  **Language Standard**:
    * **Version**: **C++20** (Strictly enforced).
    * **Mandatory Features**: Use Concepts, Ranges, Coroutines, `std::span`, `std::format`, and Modules (if supported by the specific task context).
    * **Prohibited**: Legacy C++ styles (C++98/03/11) where modern alternatives exist (e.g., prefer `std::unique_ptr` over raw pointers, `std::atomic` over `volatile`).

### 🗣️ Language & Interaction Protocol
1.  **Output Language**: **Simplified Chinese (简体中文)**.
    * All final responses, explanations, documentation, and code comments **MUST** be in Simplified Chinese.
2.  **Thinking Process**:
    * You are encouraged to use **English** for your internal reasoning, chain-of-thought, and technical analysis to ensure maximum logical accuracy.
    * **However**, the final output presented to the user must be translated into natural, professional Chinese.

### Package Management:

1. Use Scoop to manage development tools (LLVM, Ninja, CMake).

2. Use vcpkg (Manifest mode) for C++ library dependencies to ensure cross-platform reproducibility. Do NOT use system-wide package managers (like MSYS2 pacman or global headers) for project dependencies.