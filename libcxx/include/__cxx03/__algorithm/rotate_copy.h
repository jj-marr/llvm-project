//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___CXX03___ALGORITHM_ROTATE_COPY_H
#define _LIBCPP___CXX03___ALGORITHM_ROTATE_COPY_H

#include <__cxx03/__algorithm/copy.h>
#include <__cxx03/__config>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _ForwardIterator, class _OutputIterator>
inline _LIBCPP_HIDE_FROM_ABI _OutputIterator
rotate_copy(_ForwardIterator __first, _ForwardIterator __middle, _ForwardIterator __last, _OutputIterator __result) {
  return std::copy(__first, __middle, std::copy(__middle, __last, __result));
}

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___CXX03___ALGORITHM_ROTATE_COPY_H
