#include "core/StallWatch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dbghelp.h>

#include "core/Log.h"

#pragma comment(lib, "dbghelp.lib")

namespace ember {

namespace {

std::atomic<int64_t> gBeat{0};  // steady clock, nanoseconds
std::atomic<const char*> gStep{"starting"};
HANDLE gMain = nullptr;
std::jthread gWatcher;

int64_t nowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The main thread's return addresses, innermost first. It is stopped meanwhile, so nothing here may take a lock it could
// hold (no heap, no log): the unwind tables only.
size_t mainStack(DWORD64* out, size_t most) {
  if (SuspendThread(gMain) == static_cast<DWORD>(-1)) return 0;
  size_t n = 0;
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  if (GetThreadContext(gMain, &ctx)) {
    while (n < most && ctx.Rip) {
      out[n++] = ctx.Rip;
      DWORD64 base = 0;
      if (PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr)) {
        void* handlerData = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &frame, nullptr);
      } else {  // a leaf: its return address is on top of the stack
        if (!ctx.Rsp) break;
        ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
        ctx.Rsp += 8;
      }
    }
  }
  ResumeThread(gMain);
  return n;
}

void report(double stuck) {
  DWORD64 stack[48];
  const size_t n = mainStack(stack, 48);
  logWarn("stall: the main thread has not moved for {:.1f} s, in the step '{}'; where it stands ({} calls, the innermost first):", stuck,
          gStep.load(), n);
  static const bool symbols = [] {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    return SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
  }();
  for (size_t i = 0; i < n; ++i) {
    char path[MAX_PATH] = "?";
    HMODULE module = nullptr;
    DWORD64 offset = stack[i];
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(stack[i]), &module) && module) {
      GetModuleFileNameA(module, path, MAX_PATH);
      offset = stack[i] - reinterpret_cast<DWORD64>(module);
    }
    const char* file = std::strrchr(path, '\\');
    std::string name, line;
    if (symbols) {
      char buffer[sizeof(SYMBOL_INFO) + 512] = {};
      auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = 511;
      DWORD64 into = 0;
      if (SymFromAddr(GetCurrentProcess(), stack[i], &into, symbol)) name = symbol->Name;
      IMAGEHLP_LINE64 where{};
      where.SizeOfStruct = sizeof(where);
      DWORD column = 0;
      if (SymGetLineFromAddr64(GetCurrentProcess(), stack[i], &column, &where) && where.FileName) {
        const char* leaf = std::strrchr(where.FileName, '\\');
        line = std::format(" ({}:{})", leaf ? leaf + 1 : where.FileName, where.LineNumber);
      }
    }
    logWarn("stall:   {}+0x{:x} {}{}", file ? file + 1 : path, offset, name, line);
  }
}

}  // namespace

void stallBeat() { gBeat.store(nowNs(), std::memory_order_relaxed); }

void stallStep(const char* step) { gStep.store(step, std::memory_order_relaxed); }

void startStallWatch(double seconds) {
  if (gMain) return;
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &gMain,
                       THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
    gMain = nullptr;
    return;
  }
  stallBeat();
  gWatcher = std::jthread([seconds](std::stop_token stop) {
    int64_t beat = gBeat.load();
    int reports = 0;
    while (!stop.stop_requested()) {
      Sleep(250);
      const int64_t latest = gBeat.load();
      if (latest != beat) {
        if (reports > 0) logWarn("stall: the main thread moved again after {:.1f} s", static_cast<double>(latest - beat) * 1e-9);
        beat = latest, reports = 0;
        continue;
      }
      // The first report at `seconds`, then at 5x and 10x it: the same place again = stuck for good, another = slow work.
      const double stuck = static_cast<double>(nowNs() - beat) * 1e-9;
      const double due = seconds * (reports == 0 ? 1.0 : reports == 1 ? 5.0 : 10.0);
      if (reports < 3 && stuck >= due) report(stuck), ++reports;
    }
  });
}

}  // namespace ember
