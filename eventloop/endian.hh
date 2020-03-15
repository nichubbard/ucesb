/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef __ENDIAN_HH__
#define __ENDIAN_HH__

#include "byteorder_include.h"

#ifndef __BYTE_ORDER
# if defined(_BYTE_ORDER)
#  define __BYTE_ORDER     _BYTE_ORDER
#  define __LITTLE_ENDIAN  _LITTLE_ENDIAN
#  define __BIG_ENDIAN     _BIG_ENDIAN
# elif defined(BYTE_ORDER)
#  define __BYTE_ORDER     BYTE_ORDER
#  define __LITTLE_ENDIAN  LITTLE_ENDIAN
#  define __BIG_ENDIAN     BIG_ENDIAN
# else
#  ifndef DEPSRUN
#   error __BYTE_ORDER not defined
#  endif
# endif
#endif

#if __LITTLE_ENDIAN == __BIG_ENDIAN && !defined(DEPSRUN)
# error __LITTLE_ENDIAN == __BIG_ENDIAN
#endif

#endif//__ENDIAN_HH__
