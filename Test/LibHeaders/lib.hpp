#ifdef EXPORT_SHARED
#ifdef _WIN32
#define LIB _declspec(dllexport)
#else
#define LIB
#endif
#else
#define LIB
#endif


LIB const char* GetHi();