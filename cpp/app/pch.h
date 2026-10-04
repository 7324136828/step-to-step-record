#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <afxwin.h>
#include <afxwinappex.h>
#include <afxext.h>
#include <afxcmn.h>
#include <afxdialogex.h>
#include <afxole.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

#include "resource.h"
#include "native/Types.h"
