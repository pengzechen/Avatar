/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file avatar_arg.h
 * @brief Implementation of avatar_arg.h
 * @author Avatar Project Team
 * @date 2024
 */

#ifndef __MY_STDARG_H__
#define __MY_STDARG_H__

typedef __builtin_va_list va_list;

#define va_start(ap, last_param) __builtin_va_start(ap, last_param)
#define va_arg(ap, type)         __builtin_va_arg(ap, type)
#define va_end(ap)               __builtin_va_end(ap)
#define va_copy(dest, src)       __builtin_va_copy(dest, src)

#endif  // __MY_STDARG_H__
