DS2482 I²C 1-Wire® Master to Volkszaehler 'RaspberryPI deamon'
==============================================================
1WIREVZ Version 0.5 (BETA)

Hardware by Udo S.  
http://wiki.volkszaehler.org/hardware/controllers/raspberry_pi_erweiterung

![My image](http://wiki.volkszaehler.org/_media/hardware/controllers/raspi_6xs0_3x1-wire_1xir_bestueckt.png?w=200)

Backend-Software
https://github.com/volkszaehler/volkszaehler.org.git

![My image](http://wiki.volkszaehler.org/_media/software/releases/demo-screenshot.jpg?w=300)

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

Precondition: Raspian Linux (http://www.raspberrypi.org/downloads) + libcurl4-gnutls-dev + libconfig-dev

1wirevz.c 	-> /tmp ( sudo gcc -o /usr/sbin/1wirevz /tmp/1wirevz.c -lconfig -lcurl )

1wirevz.cfg 	-> /etc/  

modules   	-> /etc/  

rc.local  	-> /etc/  

1wirevz 	 	-> /etc/init.d/


Configuration
=============

$ sudo insserv 1wirevz ( register new service )

$ sudo vim /etc/1wirevz.cfg ( edit your config )

$ /etc/init.d/1wirevz start ( start the service as user, dont be root! )

