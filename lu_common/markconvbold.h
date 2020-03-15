/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __MARKCONVBOLD_H__
#define __MARKCONVBOLD_H__

/* %-conversions in error/warning/info-strings are automatically
 * boldified
 */
#define ERR_BOLD    "\033A" /* place in string before bold section */
#define ERR_ENDBOLD "\033B" /* place in string after bold section */
#define ERR_RED     "\033C" /* place in string before red text */
#define ERR_GREEN   "\033D" /* place in string before green text */
#define ERR_BLUE    "\033E" /* place in string before blue text */
#define ERR_MAGENTA "\033F" /* place in string before magenta text */
#define ERR_CYAN    "\033G" /* place in string before cyan text */
#define ERR_BLACK   "\033H" /* place in string before black text */
#define ERR_WHITE   "\033I" /* place in string before white text */
#define ERR_ENDCOL  "\033J" /* place in string after coloured section */
#define ERR_NOBOLD  "\033K" /* place in string before a %-conversion */
                            /* that should not be boldified */

char *markconvbold(const char *fmt);

void markconvbold_output(const char *fmt,int linemarkup);

#endif/*__MARKCONVBOLD_H__*/
