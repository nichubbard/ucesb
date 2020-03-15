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

#ifndef __STRNDUP_HH__
#define __STRNDUP_HH__

#include <stdlib.h>
#include <string.h>
// try to get it the normal way, hmm, won't actually
// work as outlined below (is not a macro, but anyhow)

#ifndef strndup
// Actually, since strndup is not a macro, we'll always be done...
inline char *strndup(const char *src,size_t length)
{
  // We wast memory in case the string actually is shorter...
  char *dest = (char *) malloc(length+1);
  strncpy(dest,src,length);
  dest[length]='\0'; // since strncpy would not handle this
  return dest;
}
#endif

#endif//__STRNDUP_HH__

