/**************************************************************************

DS2482 I²C 1-Wire® Master to Volkszaehler 'RaspberryPI deamon'.

https://github.com/w3llschmidt/1wirevz.git

Henrik Wellschmidt  <w3llschmidt@gmail.com>

**************************************************************************/

#define DAEMON_NAME "1wirevz"
#define DAEMON_VERSION "1.5"
#define DAEMON_BUILD "2"

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

#include <stdio.h>              /* standard library functions for file input and output */
#include <stdlib.h>             /* standard library for the C programming language, */
#include <string.h>             /* functions implementing operations on strings  */
#include <unistd.h>             /* provides access to the POSIX operating system API */
#include <sys/stat.h>           /* declares the stat() functions; umask */
#include <fcntl.h>              /* file descriptors */
#include <syslog.h>             /* send messages to the system logger */
#include <errno.h>              /* macros to report error conditions through error codes */
#include <signal.h>             /* signal processing */
#include <stddef.h>             /* defines the macros NULL and offsetof as well as the types ptrdiff_t, wchar_t, and size_t */
#include <dirent.h>		/* constructs that facilitate directory traversing */

#include <libconfig.h>          /* reading, manipulating, and writing structured configuration files */
#include <curl/curl.h>          /* multiprotocol file transfer library */

void daemonShutdown();
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);

int pidFilehandle, minterval, vzport, i, count;

const char *vzserver, *vzpath, *uuid;

char sensorid[3][32][17], vzuuid[3][32][64], crc_buffer[64], temp_buffer[64], fn[128], url[128];

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

void  daemonShutdown() {
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

	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD);
	sigaddset(&newSigSet, SIGTSTP);
	sigaddset(&newSigSet, SIGTTOU);
	sigaddset(&newSigSet, SIGTTIN);
	sigprocmask(SIG_BLOCK, &newSigSet, NULL);

	newSigAction.sa_handler = signal_handler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	sigaction(SIGHUP, &newSigAction, NULL);
	sigaction(SIGTERM, &newSigAction, NULL);
	sigaction(SIGINT, &newSigAction, NULL);

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
	
	//umask(027);

	sid = setsid();
	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	for (i = getdtablesize(); i >= 0; --i)
	{
		close(i);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	chdir(rundir);

	pidFilehandle = open(pidfile, O_RDWR|O_CREAT, 0600);

	if (pidFilehandle == -1 )
	{
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	if (lockf(pidFilehandle,F_TLOCK,0) == -1)
	{
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	sprintf(str,"%d\n",getpid());

	write(pidFilehandle, str, strlen(str));
}

void cfile() {

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

}

int count_i2cdevices() {

	int i2cdevices = 0;
	DIR * dirp;
	struct dirent * entry;

	dirp = opendir("/sys/bus/i2c/devices/");
	if (!dirp) {
		syslog ( LOG_INFO, "Error: /sys/bus/i2c/devices not found! Check kernelmodul!" );
		daemonShutdown();
	}	


		while ((entry = readdir(dirp)) != NULL) {
			if (entry->d_type == DT_LNK && strstr(entry->d_name,"i2c")) {
			i2cdevices++;
			}

		}

	closedir(dirp);

return i2cdevices-2;
}

void ds1820init() {

	int i = 0;
	for (i=1; i<=count_i2cdevices(); i++) {

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
			
				if ( ! ( strstr ( sensorid[i][count], not_found ) )) {
				
					char buffer[32];
					sprintf ( buffer, "*%s", sensorid[i][count] );
					if ( config_lookup_string( &cfg, buffer, &uuid ) == CONFIG_TRUE )
					strcpy(vzuuid[i][count], uuid);
				
				}
						
			if ( ! ( strstr ( sensorid[i][count], not_found ) )) {
			syslog( LOG_INFO, "%s (Bus: %d) (VzUUID: %s)", sensorid[i][count], i, vzuuid[i][count] );
			}
			
			count++;
			}
			
		}
		
	if (fp != NULL)
		fclose ( fp );
	}
	
}
 
void http_post( double temp, char  *vzuuid ) {

	CURL *curl;
	CURLcode curl_res;
 
	sprintf ( url, "http://%s:%d/%s/data/%s.json?value=%.2f", vzserver, vzport, vzpath, vzuuid, temp );

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if(curl)
	{ 

		FILE* devnull = NULL;
		devnull = fopen("/dev/null", "w+");

		curl_easy_setopt(curl, CURLOPT_USERAGENT, DAEMON_NAME " " DAEMON_VERSION ); 
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
		
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
	
		if( (curl_res  = curl_easy_perform(curl)) != CURLE_OK) {
		syslog ( LOG_INFO, "HTTP_POST(): %s", curl_easy_strerror(curl_res) );
		}
	
		curl_easy_cleanup(curl);
		fclose ( devnull );
		
	}

curl_global_cleanup();
}

double ds1820read(char *sensorid) {

	FILE *fp;	

	sprintf(fn, "/sys/bus/w1/devices/%s/w1_slave", sensorid );

	if  ( (fp = fopen ( fn, "r"  )) == NULL ) {
	return(-1);
	}
	
	else 
	
	{ 
	
		fgets( crc_buffer, sizeof(crc_buffer), fp );
		if ( !strstr ( crc_buffer, crc_ok ) ) 
	 	{
		
			syslog(LOG_INFO, "CRC check failed, SensorID: %s", sensorid);
			
		fclose ( fp );
		return(-1);
		}
		
		else 
		
		{ 
		
		fgets( temp_buffer, sizeof(temp_buffer), fp );
		fgets( temp_buffer, sizeof(temp_buffer), fp );
		
			/**************************************************************************
			char *t;
			t = strndup ( temp_buffer +29, 5 ) ;
			temp = atof(t)/1000;
			**************************************************************************/
			
			char *pos = strstr(temp_buffer, "t=");

			if (pos == NULL)
				return -1;

			pos += 2;
			
			temp = atof(pos)/1000;
			
			
		fclose ( fp );
		http_post(temp, vzuuid[i][count]);

		}

	}
	
}

int main() { 

    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);

	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	syslog(LOG_INFO, "DS2482 I²C 1-Wire® Master to Volkszaehler deamon %s (%s) %d", DAEMON_VERSION, DAEMON_BUILD, count_i2cdevices());

	cfile();
	
	ds1820init();
	
	char pid_file[16];
	sprintf ( pid_file, "/tmp/%s.pid", DAEMON_NAME );
	daemonize( "/tmp/", pid_file );
				
	while(1) {
	
			i = 0;
			for (i=1; i<=count_i2cdevices(); i++) {
			
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
						
						}

					count++;
					}
					
				}
				
			if (fp != NULL)
				fclose ( fp );	
			}
			
	sleep(minterval);
	}
	
return(0);
}
