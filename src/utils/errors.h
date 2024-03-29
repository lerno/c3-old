#pragma once

// Copyright (c) 2019 Christoffer Lerno. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

void eprintf(const char *format, ...);
void error_exit(const char *format, ...);

#define FATAL_ERROR(_string, ...) do { error_exit("FATAL ERROR at %s:%d: " _string, __func__, __LINE__, ##__VA_ARGS__); } while(0)

#define ASSERT(_condition, _string, ...) while (!(_condition)) { FATAL_ERROR(_string, ##__VA_ARGS__); }

#define UNREACHABLE FATAL_ERROR("Cannot reach %s:%d", __func__, __LINE__);

#define TODO FATAL_ERROR("TODO reached", __func__, __LINE__);

#define TEST_ASSERT(_condition, _string, ...) while (!(_condition)) { FATAL_ERROR(_string, ##__VA_ARGS__); }

#define EXPECT(_string, _value, _expected) \
 do { long long __tempval1 = _value; long long __tempval2 = _expected; \
    TEST_ASSERT(__tempval1 == __tempval2, "Checking " _string ": expected %lld but was %lld.", __tempval2, __tempval1); } while(0)


#ifndef NDEBUG
#define DEBUG_LOG(_string, ...) eprintf("-- DEBUG: "); eprintf(_string, ##__VA_ARGS__); eprintf("\n");
#else
#define DEBUG_LOG(_string, ...)
#endif
#define LOG_FUNC DEBUG_LOG("%s entered", __func__);

