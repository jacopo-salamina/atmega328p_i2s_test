#pragma once

/*
 * Unfortunately, the AVR toolchain doesn't provide many standard C++ headers.
 * This header file is a replacement for <type_traits>, and it only provides the
 * template helpers 'enable_if' and 'enable_if_t'.
 * 
 * Implementation copied-n-pasted from
 * https://en.cppreference.com/w/cpp/types/enable_if
 */

template<bool B, class T = void>
struct enable_if {};
 
template<class T>
struct enable_if<true, T> { typedef T type; };

template< bool B, class T = void >
using enable_if_t = typename enable_if<B,T>::type;
