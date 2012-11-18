/**************************************************************************

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

**************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libconfig.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <curl/curl.h>

#define DAEMON_NAME "1wirevz"

void daemonShutdown();
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);

int pidFilehandle, minterval, vzport;

const char *vzserver, *vzpath;

char sensorid[16];


void signal_handler(int sig)
{
	switch(sig)
	{
		case SIGHUP:
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
		case SIGINT:
		case SIGTERM:
		syslog(LOG_INFO, "Daemon exiting");
		daemonShutdown();
		exit(EXIT_SUCCESS);
		break;
		default:
		syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
		break;
	}
}

void daemonShutdown()
{
		close(pidFilehandle);
		remove("/tmp/1wirevz.pid");
}

void daemonize(char *rundir, char *pidfile)
{
	int pid, sid, i;
	char str[10];
	struct sigaction newSigAction;
	sigset_t newSigSet;

	if (getppid() == 1)
	{
		return;
	}

	/* Signal mask - block */
	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD);
	sigaddset(&newSigSet, SIGTSTP);
	sigaddset(&newSigSet, SIGTTOU);
	sigaddset(&newSigSet, SIGTTIN);
	sigprocmask(SIG_BLOCK, &newSigSet, NULL);

	/* Signal handler */
	newSigAction.sa_handler = signal_handler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	/* Signals to handle */
	sigaction(SIGHUP, &newSigAction, NULL);
	sigaction(SIGTERM, &newSigAction, NULL);
	sigaction(SIGINT, &newSigAction, NULL);

	/* Fork*/
	pid = fork();

	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}

	if (pid > 0)
	{
		printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}
	
	umask(027); /* file permissions 750 */

	sid = setsid();
	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	for (i = getdtablesize(); i >= 0; --i)
	{
		close(i);
	}

	/* Route I/O connections */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	chdir(rundir); /* change running directory */

	/* Ensure only one copy */
	pidFilehandle = open(pidfile, O_RDWR|O_CREAT, 0600);

	if (pidFilehandle == -1 )
	{
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	/* Try to lock file */
	if (lockf(pidFilehandle,F_TLOCK,0) == -1)
	{
		/* Couldn't get lock on lock file */
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	/* Get and format PID */
	sprintf(str,"%d\n",getpid());

	/* write pid to lockfile */
	write(pidFilehandle, str, strlen(str));
}

int cfile(void)
{
	config_t cfg;
	config_setting_t *setting;
	config_init(&cfg);

	int chdir(const char *path);

	/* Read the config, check for errors, report it and exit. */
	chdir ("/etc");

	if(!config_read_file(&cfg, DAEMON_NAME".cfg"))
	{
		syslog(LOG_INFO, "Config error > /etc/%s - %s\n", config_error_file(&cfg),config_error_text(&cfg));
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}


	if (!config_lookup_string(&cfg, "vzserver", &vzserver))
	{
		syslog(LOG_INFO, "Missing 'VzServer' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzServer:%s", vzserver);

	if (!config_lookup_int(&cfg, "vzport", &vzport))
	{
		syslog(LOG_INFO, "Missing 'VzPort' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPort:%d", vzport);


	if (!config_lookup_string(&cfg, "vzpath", &vzpath))
	{
		syslog(LOG_INFO, "Missing 'VzPath' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPath:%s", vzpath);

	if (!config_lookup_int(&cfg, "interval", &minterval))
	{
		syslog(LOG_INFO, "Missing 'Interval' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "Metering interval:%d sec", minterval);

	//config_destroy(&cfg);

	return(EXIT_SUCCESS);
}

int ds2482_sysfs_init(void)
{
	FILE *f = fopen("/sys/bus/w1/devices/w1_bus_master1/w1_master_slaves", "r");
	fgets(sensorid, 16, f);            
	fclose(f);
	syslog(LOG_INFO, "1W devices found: %s", sensorid);
}

int ds1820read(void)
{
	char crc_buffer[40];
	char temp_buffer[40];
	char crccheck[] = "YES";

	char format[] = "/sys/bus/w1/devices/w1_bus_master1/%s/w1_slave";
	char filename[sizeof format+16];

	sprintf ( filename, format, sensorid );

	//syslog(LOG_INFO, "%s", filename);

	FILE *fp = fopen ( filename, "r" );

		fgets( crc_buffer, sizeof(crc_buffer), fp );
		
		if ( !strstr ( crc_buffer, crccheck ) )
		{
			syslog(LOG_INFO, "CRC check failed, SensorID: %s", sensorid);
			return(-1);
		}
		else
		{
			fgets( temp_buffer, sizeof(temp_buffer), fp );
			fgets( temp_buffer, sizeof(temp_buffer), fp );
			syslog(LOG_INFO, "%s", temp_buffer);
		}

	fclose ( fp );
	return(EXIT_SUCCESS);
}

int http_post(void)
{

        char format[] = "http://%s:%d/%s/data/52196840-2ef9-11e2-853d-fff0722808ce.json?value=18.234";
        char url[sizeof format+128];

        sprintf ( url, format, vzserver, vzport, vzpath );

		CURL *curl;
		CURLcode res;

		curl_global_init(CURL_GLOBAL_ALL);

		curl = curl_easy_init();
		if(curl) {

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");


		res = curl_easy_perform(curl);

		if(res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));

		curl_easy_cleanup(curl);
		}
		curl_global_cleanup();
		return 0;

}


int main(void)
{
	// Dont talk, just kiss!
	fclose(stdout);
	fclose(stderr);

	// Start Logging
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	// Hello world!
	syslog(LOG_INFO, "DS2482 I²C 1-Wire® Master to Volkszaehler deamon 1.0");

	// Check and process the config file (/etc/1wirevz.cfg) */
	cfile();

	// DS2482 sysfs initalisieren; Sensor-IDs einlesen
	ds2482_sysfs_init();

	// Deamonize
	daemonize("/tmp/", "/tmp/1wirevz.pid");

	// Mainloop
	while (1)
	{
		ds1820read();
		http_post();
		sleep(minterval);
	}
}
