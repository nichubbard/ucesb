/* This file is part of DRASI - a data acquisition data pump.
 *
 * Copyright (C) 2017  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "format_prefix.hh"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

void format_prefix(char *buf, size_t size,
		   double val, int max_width,
		   format_diff_info *info)
{
  /* First find out what prefix to use. */

  double x = val;
  int min_prefix = 0;
  int max_prefix = 0;
  double limit = 20000;
  double nextlimit = 20000;

  if (max_width == 9)
    {
      limit = 999000000.;
      nextlimit = 99900000.;
    }
  else if (max_width == 10)
    {
      limit = 9990000000.;
      nextlimit = 999000000.;
    }
  else if (max_width == -6)
    {
      max_width = 6;
      limit = 900000.;
      nextlimit = 90000.;
    }

  if (isfinite(x))
    {
      while (x > limit && min_prefix < 255)
	{
	  limit = nextlimit;
	  x /= 1000;
	  min_prefix++;
	}

      max_prefix = min_prefix;
      
      if (max_prefix < info->_prefix)
	{
	  if (info->_prefix_age < 10)
	    {
	      info->_prefix_age++;
	      max_prefix = info->_prefix;
	    }
	  else
	    {
	      max_prefix = info->_prefix-1;
	    }
	}
    }

  /* printf ("%d %d\n", min_prefix, max_prefix); */

  if (val == 0.0)
    snprintf (buf, size, "0");
  else
    {
      double print_val;
      int print_prefix, i;    
      int decimal;

      int prefix, extra_prefix;

      /* If we do not manage to get the size down, we in turn try to
       * a) strip leading 0 from 0.
       * b) remove the decimal value
       * c) use a higher prefix.
       */

      for (prefix = max_prefix; prefix >= min_prefix; prefix--)
	{
      for (extra_prefix = 0; extra_prefix <= 1; extra_prefix++)
	{      
	  const char prefixes[] = "\0kMGTPEZY?";
	  
	  print_val = val;
	  print_prefix = 0;
 
	  for (i = 0; i < prefix + extra_prefix && prefixes[i+1]; i++)
	    {
	      double div_val;
	      
	      div_val = print_val / 1000;

	      print_val = div_val;
	      print_prefix++;
	    }

	  if (!prefixes[print_prefix])
	    {
	      if (print_val < .1)
		decimal = 2;
	      else if (print_val < 1)
		decimal = 1;
	      else if (print_val < 2 && print_val != 1)
		decimal = 1;
	      else
		decimal = 0;
	    }
	  else if ((max_width != 8) && print_val > 999)
	    decimal = 0;
	  else
	    decimal = 1;

	  /* printf ("%d %d %d\n", prefix, print_prefix, decimal); */

	  /* First try as determined, then without decimal. */
	  for ( ; decimal >= 0; decimal--)
	    {
	      char *p;
	      
	      snprintf (buf, size,
			"%.*f%c",
			decimal,
			print_val, prefixes[print_prefix]);

	      /* If we only printed 0s, then it is no good. */
	      p = buf;
	      /* Skip past all 0 and . */
	      while (*p == '0' || *p == '.')
		p++;
	      /* Next better be a digit. */
	      if (*p < '1' || *p > '9')
		break;

	      /* Do not use return value of snprintf, as we may print a 0 %c. */

	      if (strlen(buf) <= (size_t) max_width)
		goto accept;

	      /* Try to remove leading 0 in 0. */
      
	      if (strncmp(buf,"0.",2) == 0)
		{
		  memmove(buf, buf+1, strlen(buf));

		  if (strlen(buf) <= (size_t) max_width)
		    goto accept;
		}
	    }
	}
	}

      /* Nothing created an acceptable solution. */
      snprintf (buf, size, "x");
      return;     

    accept:
      if (print_prefix != info->_prefix)
	{
	  info->_prefix = (uint8_t) print_prefix;
	  info->_prefix_age = 0;
	}
    }
}

