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

d := dataset('~local::rkc::person', { string15 name, boolean myBool; string20 forename, unsigned8 filepos{virtual(fileposition)} }, flat);

x := 'x' : stored('x');

i := index(d, { d } ,'\\home\\person.name_first.key.' + x);

sequential(
    output(i(keyed(name = 'Gavin'),keyed(myBool = false)));
    output(i(keyed(name = 'Gavin'),keyed(not myBool)));
    output(i(keyed(name = 'Gavin'),keyed(myBool = true)));
    output(i(keyed(name = 'Gavin'),keyed(myBool)))
);


