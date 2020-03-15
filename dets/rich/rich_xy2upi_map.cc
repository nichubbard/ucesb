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

#include "rich_xy2upi_map.hh"

#include "error.hh"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

rich_xy2upi_map::rich_xy2upi_map()
{
  for (int x = 0; x < 96; x++)
    for (int y = 0; y < 96; y++)
      _xy2upi._xy[x][y].u16 = (uint16) -1; // invalid, as sector will be 7 (not in 0..5)

  for (int rc = 0; rc < 2; rc++)
    for (int p = 0; p < 8; p++)
      for (int m = 0; m < 8; m++)
	{
	  _upi2mod._used[rc][p][m] = 0;
	  for (int ch = 0; ch < 64; ch++)
	    {
	      _upi2xy._upi[rc][p][m][ch]._x = (uint8) -1;
	      _upi2xy._upi[rc][p][m][ch]._y = (uint8) -1;

	    }
	}

  for (int cpmc = 0; cpmc < 2*8*8*64; cpmc++)
    {
      _upi2index._upif[cpmc]._index_module = (uint16) -1;
      _upi2index._upif[cpmc]._index_sector = (uint16) -1;
    }
}

void rich_xy2upi_map::setup(const char *filename)
{
  printf ("Setup RICH xy2upi... ");

  FILE *fid = fopen(filename,"r");

  if (fid == NULL)
    ERROR("Failed to open RICH xy2upi map: %s",filename);

  int items = 0;

  while (!feof(fid))
    {
      char line[1024];

      int controller, port, module, channel;
      int x, y;

      if (!fgets(line,sizeof(line),fid))
	break;

      int n = sscanf(line,"%d %d %d %d %d %d",
		     &controller, &port, &module, &channel,
		     &x, &y);

      if (n != 6)
	ERROR("Malformed RICH xy2upi: %s",line);

      /* Make sure all indices are within range. */

#define CHECK_RANGE(value,max) \
if ((value)<0||(value)>=(max)) \
  ERROR("Index " #value "(%d) outside range (>= %d): %s",(value),(max),line);

      CHECK_RANGE(controller,2);
      CHECK_RANGE(port,8);
      CHECK_RANGE(module,8);
      CHECK_RANGE(channel,64);
      CHECK_RANGE(x,96);
      CHECK_RANGE(y,96);

      /* Make sure the item is not known, neither in x,y, nor in upi */

      rich_map_upi_item *upi = &_xy2upi._xy[x][y];

      if (upi->u16 != (uint16) -1)
	ERROR("Pad already known for this x,y: %s",line);

      upi->sector     = 0;
      upi->controller = (uint16) controller;
      upi->port       = (uint16) port;
      upi->module     = (uint16) module;
      upi->channel    = (uint16) channel;

      /* Do we know the module? */

      _upi2mod._used[controller][port][module]++;

      rich_map_xy_item *xy = &_upi2xy._upi[controller][port][module][channel];

      if (xy->_x != (uint8) -1)
	ERROR("Pad already known for this upi: %s",line);

      xy->_x = (uint8) x;
      xy->_y = (uint8) y;

      //printf ("%d %d %d %d : %d %d\n",controller, port, module, channel, x, y);

      items++;
    }
  /*
  for (int i = 0; i < 2*8*8*64; i++)
    printf ("%5d: %d %d\n",
	    i,
	    _upi2xy._upif[i]._x,
	    _upi2xy._upif[i]._y);
  */

  // Calculate re-ordered indices within sector (sorted by y,x)

  {
    uint16 next_index = 0;

    for (int y = 0; y < 96; y++)
      for (int x = 0; x < 96; x++)
	if (_xy2upi._xy[x][y].u16 != (uint16) -1)
	  {
	    uint16 cpmc = _xy2upi._xy[x][y].cpmc;
	    // Has a value, so use next index
	    _upi2index._upif[cpmc]._index_sector = next_index++;
	  }
  }

  // Calculate re-ordered indices within module (sorted by y,x)

  for (int i = 0; i < 2*8*8; i++)
    {
      uint16 next_index = 0;
      int rows = 0;
      int last_row = -1;

      for (int y = 0; y < 96; y++)
	for (int x = 0; x < 96; x++)
	  if (_xy2upi._xy[x][y].u16 != (uint16) -1 &&
	      _xy2upi._xy[x][y].cpm == i)
	    {
	      uint16 cpmc = _xy2upi._xy[x][y].cpmc;
	      // Has a value, so use next index
	      _upi2index._upif[cpmc]._index_module = next_index++;

	      if (y != last_row)
		{
		  rows++;
		  last_row = y;
		}
	    }

      /*
      printf ("mod %3d: %2d (%d, rows: %d)\n",
	      i,_upi2mod._usedf[i],next_index,rows);
      */
    }

  printf ("%d items\n",items);

  fclose(fid);
}
