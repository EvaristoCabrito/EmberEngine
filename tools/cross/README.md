# Building jogo/Sandbox.exe without Windows

`jogo/Sandbox.exe` + `jogo/shaders/*.spv` are a ready-to-run Windows build made on Linux, so JOGAR.bat opens the
game without Visual Studio. Rebuild them after changing engine/app/shader code:

- Compiler: zig (`pip install --target <dir> ziglang`), target `x86_64-windows-gnu` (clang + libc++ + mingw-w64).
  Wrappers `zig-cc`, `zig-c++`, `zig-ar`, `zig-ranlib`, `zig-rc` that call `zig <tool> -target x86_64-windows-gnu`.
- Toolchain file: `CMAKE_SYSTEM_NAME Windows`, compilers = the wrappers.
- Vulkan headers: KhronosGroup/Vulkan-Headers tag `vulkan-sdk-1.4.363.0`, plus `vk_enum_string_helper.h` from
  Vulkan-Utility-Libraries (same tag) copied into `include/vulkan/`. `Vulkan_LIBRARY` / `Vulkan_dxc_LIBRARY` can be
  empty dummy files (volk loads Vulkan at run time).
- DXC: microsoft/DirectXShaderCompiler built from source on Linux
  (`cmake -GNinja -C cmake/caches/PredefinedParams.cmake -DCMAKE_BUILD_TYPE=Release ..; ninja dxc`), passed as
  `Vulkan_dxc_EXECUTABLE`.
- Configure with `-DCMAKE_CXX_SCAN_FOR_MODULES=OFF`, then `ninja Sandbox`; copy `bin/Sandbox.exe` and
  `bin/shaders/*.spv` into `jogo/`.
