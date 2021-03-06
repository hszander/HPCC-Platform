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

#option ('globalFold', false);
string x := 'abc' : stored('x');
string y := 'def' : stored('y');
integer four := 4 : stored('four');
integer five := 5 : stored('five');
integer six := 6 : stored('six');
integer seven := 7 : stored('seven');

x[..5];
x[3..];
x[2..4];
x[1..7];
x[1..five];
x[..six];
x[four..seven];
x[five..];

x[..5]+x[3..]+x[2..4]+x[1..7];
x[1..five]+x[..six]+x[four..seven]+x[five..];
