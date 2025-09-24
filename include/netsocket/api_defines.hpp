
#pragma once

#if (defined _WIN32 || defined __CYGWIN__) && defined(__GNUC__)
#	define NETSOCKET_IMPORT_API __declspec(dllimport)
#	define NETSOCKET_EXPORT_API __declspec(dllexport)
#else
#	define NETSOCKET_IMPORT_API __attribute__((visibility("default")))
#	define NETSOCKET_EXPORT_API __attribute__((visibility("default")))
#endif

#ifdef NETSOCKET_BUILD_STATIC_LIBRARY
#	define NETSOCKET_API
#elif defined(NETSOCKET_BUILD_DYNAMIC_LIBRARY)
#	define NETSOCKET_API NETSOCKET_EXPORT_API
#elif defined(NETSOCKET_USE_DYNAMIC_LIBRARY)
#	define NETSOCKET_API NETSOCKET_IMPORT_API
#elif defined(NETSOCKET_USE_STATIC_LIBRARY)
#	define NETSOCKET_API
#else
#	define NETSOCKET_API
#endif

