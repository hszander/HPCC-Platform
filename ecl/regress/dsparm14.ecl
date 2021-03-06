/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

ds1 := dataset('ds1', {integer m1; }, THOR);
ds2 := dataset('ds2', {integer m2; }, THOR);

r1 := record integer n; end;
r2 := record integer n; end;

dataset f(virtual dataset(r1) d1, virtual dataset(r2) d2) := d1(n = count(d2(n=10)));

integer g(virtual dataset d) := count(d);

h(virtual dataset d1, virtual dataset d2) := g(f(d1, d2));

ct := h(ds1{n:=m1;}, ds2{n:=m2;});

ct;
