
#pragma once
#include <netsocket/api_defines.hpp>
#if !defined(NETSOCKET_RELEASE) && !defined(NETSOCKET_DEBUG)
#   warning "None of NETSOCKET_RELEASE && NETSOCKET_DEBUG is defined; using NETSOCKET_DEBUG"
#   define NETSOCKET_DEBUG
#endif

