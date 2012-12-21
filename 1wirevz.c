/**************************************************************************

Part of DS2482 I²C 1-Wire® Master to Volkszaehler 'RaspberryPI deamon'.

Version 0.5

sudo gcc -o /usr/sbin/1wirevz /home/pi/1wirevz/1wirevz.c -lconfig -lcurl 

https://github.com/w3llschmidt/1wirevz.git
https://github.com/volkszaehler/volkszaehler.org.git

Henrik Wellschmidt  <w3llschmidt@gmail.com>

**************************************************************************/

#define DAEMON_NAME "1wirevz"
#define DAEMON_VERSION "0.6"


/**************************************************************************

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
#include <curl/curl.h>
#include <linux/i2c-dev.h>

void daemonShutdown();
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);

int pidFilehandle, minterval, vzport, i, count;

const char *vzserver, *vzpath, *uuid;

char sensorid[3][64][17], vzuuid[3][64][128], crc_buffer[64], temp_buffer[64], fn[64], url[128];

char crc_ok[] = "YES";
char not_found[] = "not found.";

double temp;

config_t cfg;

void signal_handler(int sig) {
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

void daemonShutdown() {
		close(pidFilehandle);
		remove("/tmp/1wirevz.pid");
}

void daemonize(char *rundir, char *pidfile) {
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

int cfile() {

	config_setting_t *setting;
	config_init(&cfg);

	int chdir(const char *path);

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
	syslog(LOG_INFO, "VzServer: %s", vzserver);

	if (!config_lookup_int(&cfg, "vzport", &vzport))
	{
		syslog(LOG_INFO, "Missing 'VzPort' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPort: %d", vzport);


	if (!config_lookup_string(&cfg, "vzpath", &vzpath))
	{
		syslog(LOG_INFO, "Missing 'VzPath' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "VzPath: %s", vzpath);

	if (!config_lookup_int(&cfg, "interval", &minterval))
	{
		syslog(LOG_INFO, "Missing 'Interval' setting in configuration file.");
		config_destroy(&cfg);
		daemonShutdown();
		exit(EXIT_FAILURE);
	}
	else
	syslog(LOG_INFO, "Metering interval: %d sec", minterval);
	
return ( EXIT_SUCCESS);
}

int ds1820init() {

	int i = 0;
	for (i=1; i<=3; i++) {

		char fn[64];
		sprintf ( fn, "/sys/bus/w1/devices/w1_bus_master%d/w1_master_slaves", i );

		FILE *fp;	
		if  ( (fp = fopen ( fn, "r" )) == NULL ) {
		syslog(LOG_INFO, "%s", strerror(errno));					
		}
		else
		{
			count = 1;
			
			while ( fgets ( sensorid[i][count], sizeof(sensorid[i][count]), fp ) != NULL ) {
			sensorid[i][count][strlen(sensorid[i][count])-1] = '\0';
			
						if ( !( strstr ( sensorid[i][count], not_found ) )) {
						
							char buffer[32];
							sprintf ( buffer, "*%s", sensorid[i][count] );
							if ( config_lookup_string( &cfg, buffer, &uuid ) == CONFIG_TRUE )
							strcpy(vzuuid[i][count], uuid);
						
						}
						
			syslog( LOG_INFO, "%s (Bus: %d) (VzUUID: %s)", sensorid[i][count], i, vzuuid[i][count] );
			
			count++;
			}
			
		}
		
	fclose ( fp );
	}
	
return ( EXIT_SUCCESS);
}

double ds1820read(char *sensorid) {

	sprintf(fn, "/sys/bus/w1/devices/%s/w1_slave", sensorid );

	FILE *fp;	
	if  ( (fp = fopen ( fn, "r" )) == NULL ) {
	return(1);
	}
	else 
	{
		fgets( crc_buffer, sizeof(crc_buffer), fp );
		if ( !strstr ( crc_buffer, crc_ok ) ) {
			syslog(LOG_INFO, "%s", crc_buffer);
			syslog(LOG_INFO, "CRC check failed, SensorID: %s", sensorid);
		}
		else 
		{
		
		fgets( temp_buffer, sizeof(temp_buffer), fp );
		fgets( temp_buffer, sizeof(temp_buffer), fp );
		
			char *t;
			t = strndup ( temp_buffer +29, 5 ) ;
			temp = atof(t)/1000;
			return(temp);
			
		}
		
	fclose ( fp );	
	}

return ( EXIT_SUCCESS);
}

int http_post( double temp, char *vzuuid ) {

	sprintf ( url, "http://%s:%d/%s/data/%s.json?value=%.2f", vzserver, vzport, vzpath, vzuuid, temp );
			
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if(curl) 
	{
		FILE* devnull = NULL;
		devnull = fopen("/dev/null", "w+");

<<<<<<< HEAD
		curl_easy_setopt(curl, CURLOPT_USERAGENT, DAEMON_NAME " " DAEMON_VERSION ); 
=======
		curl_easy_setopt(curl, CURLOPT_USERAGENT, DAEMON_NAME " " DAEMON_VERSION );
>>>>>>> 894924dd525702635f8accbd4ad4f4bc8db997b1
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);

		res = curl_easy_perform(curl);

			if(res != CURLE_OK)
			syslog(LOG_INFO, "http_post() %s", curl_easy_strerror(res)); 

		curl_easy_cleanup(curl);

		fclose ( devnull );
	}
	curl_global_cleanup();
	
return ( EXIT_SUCCESS);
}

int main() {

	// Dont talk, just kiss!
	fclose(stdout);
	fclose(stderr);

	// Start Logging
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	// Hello world!
	syslog(LOG_INFO, "DS2482 I²C 1-Wire® Master to Volkszaehler deamon %s", DAEMON_VERSION);
	
	// Check and process the config file (/etc/1wirevz.cfg) */
	cfile();
	
	// Sensoren einlesen
	ds1820init();

	// Deamonize
	daemonize("/tmp/", "/tmp/1wirevz.pid");
						
	// Mainloop
	
	while(1) {
	
			i = 0;
			for (i=1; i<=3; i++) {
			
				sprintf ( fn, "/sys/bus/w1/devices/w1_bus_master%d/w1_master_slaves", i );
				
				FILE *fp;	
				if  ( (fp = fopen ( fn, "r" )) == NULL ) 
				{
				syslog(LOG_INFO, "%s", strerror(errno));
				}
				else
				{
				
					count = 1;
					while ( fgets ( sensorid[i][count], sizeof(sensorid[i][count]), fp ) != NULL ) {
					sensorid[i][count][strlen(sensorid[i][count])-1] = '\0';
					
						if ( !( strstr ( sensorid[i][count], not_found ) )) {
						ds1820read(sensorid[i][count]);
		
						http_post(temp, vzuuid[i][count]);
						}

					count++;
					}
					
				}
				
			fclose ( fp );	
			}
			
	sleep(minterval);
	}
	
return ( EXIT_SUCCESS);
}
