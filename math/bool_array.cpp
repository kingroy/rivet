/**********************************************************************
Copyright 2014-2016 The RIVET Developers. See the COPYRIGHT file at
the top-level directory of this distribution.

This file is part of RIVET.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/
//
// Created by Bryn Keller on 11/22/16.
//

#include "bool_array.h"

bool_array::bool_array(unsigned long rows, unsigned long cols)
    : cols(cols)
    , array(new bool[rows * cols])
{
}

bool& bool_array::at(unsigned long row, unsigned long col) { return array[row * cols + col]; }
