// RegexShim.cpp — MSVC regex ABI compatibility shim.
// CommonLibSSE-ng was compiled with an older MSVC that exported
// __std_regex_transform_primary_char from the C++ runtime.
// Current MSVC declares it in <regex> but doesn't export it from the runtime lib.
// This stub satisfies the linker without affecting functionality.

#include <cstddef>

// MSVC 19.50+ declares the symbol in <regex> with __stdcall and 5 args.
// We must match that signature exactly to avoid "cannot overload extern C" errors.
// The guard in <regex> is: #if defined(_CPPRTTI) && !defined(_M_CEE_PURE)
#if _MSC_VER >= 1950

struct _Collvec;  // forward-declare to avoid pulling in <regex>

extern "C" {
    size_t __stdcall __std_regex_transform_primary_char(
        char* /*_First1*/, char* /*_Last1*/,
        const char* /*_First2*/, const char* /*_Last2*/,
        const _Collvec* /*_Vector*/) noexcept
    {
        return 0;
    }

    size_t __stdcall __std_regex_transform_primary_wchar_t(
        wchar_t* /*_First1*/, wchar_t* /*_Last1*/,
        const wchar_t* /*_First2*/, const wchar_t* /*_Last2*/,
        const _Collvec* /*_Vector*/) noexcept
    {
        return 0;
    }
}

#else
// Older MSVC (19.44-19.49): __cdecl, 3-arg signature.
extern "C" {
    size_t __cdecl __std_regex_transform_primary_char(
        const void* /*locale_name*/,
        void* /*out*/,
        size_t /*out_count*/)
    {
        return 0;
    }
}
#endif
