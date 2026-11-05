# dav1d (VideoLAN's AV1 decoder, BSD-2) compiled by our own target: upstream builds with Meson, which this tree does not use.
# Mirrors dav1d's src/meson.build for 64-bit Windows with MSVC: the C decoder (no assembly: that needs NASM), 8-bit only (the
# films are 8-bit 4:2:0), its own threads (src/win32/thread.c) and its own <stdatomic.h> for MSVC (include/compat/msvc).
set(DAV1D_DIR ${dav1d_SOURCE_DIR})
set(DAV1D_GEN ${CMAKE_BINARY_DIR}/dav1d-config)

# config.h: what dav1d's meson.build writes for this machine (every switch the sources test; anything absent reads as 0)
file(WRITE ${DAV1D_GEN}/config.h.new [=[
/* written by cmake/Dav1d.cmake (dav1d's meson.build for x86-64 Windows, MSVC, no assembly, 8-bit only) */
#define ARCH_AARCH64 0
#define ARCH_ARM 0
#define ARCH_LOONGARCH 0
#define ARCH_LOONGARCH64 0
#define ARCH_PPC64LE 0
#define ARCH_RISCV 0
#define ARCH_X86 1
#define ARCH_X86_32 0
#define ARCH_X86_64 1
#define CONFIG_16BPC 0
#define CONFIG_8BPC 1
#define CONFIG_LOG 1
#define ENDIANNESS_BIG 0
#define HAVE_ASM 0
#define HAVE_ALIGNED_ALLOC 0
#define HAVE_C11_GENERIC 0
#define HAVE_CLOCK_GETTIME 0
#define HAVE_DLSYM 0
#define HAVE_ELF_AUX_INFO 0
#define HAVE_GETAUXVAL 0
#define HAVE_IO_H 1
#define HAVE_MEMALIGN 0
#define HAVE_POSIX_MEMALIGN 0
#define HAVE_PTHREAD_GETAFFINITY_NP 0
#define HAVE_PTHREAD_NP_H 0
#define HAVE_PTHREAD_SETAFFINITY_NP 0
#define HAVE_PTHREAD_SETNAME_NP 0
#define HAVE_PTHREAD_SET_NAME_NP 0
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 0
#define TRIM_DSP_FUNCTIONS 0
#define _WIN32_WINNT 0x0601
#define UNICODE 1
#define _UNICODE 1
#define _CRT_DECLARE_NONSTDC_NAMES 1
#define _FILE_OFFSET_BITS 64
#define fseeko _fseeki64
#define ftello _ftelli64
]=])
file(WRITE ${DAV1D_GEN}/vcs_version.h.new "/* written by cmake/Dav1d.cmake */\n#define DAV1D_VERSION \"1.5.4\"\n")
foreach(f config.h vcs_version.h)  # rewritten only when they change, so a configure does not rebuild the decoder
  file(COPY_FILE ${DAV1D_GEN}/${f}.new ${DAV1D_GEN}/${f} ONLY_IF_DIFFERENT)
endforeach()

set(DAV1D_SOURCES
  cdf.c cpu.c ctx.c data.c decode.c dequant_tables.c getbits.c intra_edge.c itx_1d.c lf_mask.c lib.c log.c mem.c msac.c obu.c pal.c
  picture.c qm.c ref.c refmvs.c scan.c tables.c thread_task.c warpmv.c wedge.c win32/thread.c)
set(DAV1D_TMPL_SOURCES  # compiled once per bit depth with BITDEPTH defined (8 only here)
  cdef_apply_tmpl.c cdef_tmpl.c fg_apply_tmpl.c filmgrain_tmpl.c ipred_prepare_tmpl.c ipred_tmpl.c itx_tmpl.c lf_apply_tmpl.c
  loopfilter_tmpl.c looprestoration_tmpl.c lr_apply_tmpl.c mc_tmpl.c recon_tmpl.c)
list(TRANSFORM DAV1D_SOURCES PREPEND ${DAV1D_DIR}/src/)
list(TRANSFORM DAV1D_TMPL_SOURCES PREPEND ${DAV1D_DIR}/src/)

set(DAV1D_INCLUDES ${DAV1D_GEN} ${DAV1D_DIR} ${DAV1D_DIR}/include/dav1d ${DAV1D_DIR}/include ${DAV1D_DIR}/include/compat/msvc)
add_library(dav1d_bitdepth_8 OBJECT ${DAV1D_TMPL_SOURCES})
target_compile_definitions(dav1d_bitdepth_8 PRIVATE BITDEPTH=8)
add_library(dav1d STATIC ${DAV1D_SOURCES} $<TARGET_OBJECTS:dav1d_bitdepth_8>)
foreach(t dav1d dav1d_bitdepth_8)
  target_include_directories(${t} PRIVATE ${DAV1D_INCLUDES})
  if(MSVC)
    target_compile_options(${t} PRIVATE /W0 /wd4028 /wd4090 /wd4996)
    target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endforeach()
target_include_directories(dav1d SYSTEM INTERFACE ${DAV1D_DIR}/include)
