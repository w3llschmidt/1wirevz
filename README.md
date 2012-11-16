1wirevz - PROTOTYPE - dont use at this time!
============================================

Part of DS2482 I²C 1-Wire® Master to Volkszaehler 'RaspberryPI deamon'.

Version 0.2

sudo gcc -o /usr/sbin/1wirevz /home/pi/1wirevz/1wirevz.c -lconfig -lcurl 

https://github.com/w3llschmidt/1wirevz.git
https://github.com/volkszaehler/volkszaehler.org.git

Henrik Wellschmidt  <w3llschmidt@gmail.com>

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

Installation
============

sudo gcc -o /usr/sbin/1wirevz /tmp/1wirevz.c -lconfig -lcurl

1wirevz.cfg		-> /etc/ 
modules  		-> /etc/
rc.local  		-> /etc/
1wirevz 		-> /etc/init.d/
