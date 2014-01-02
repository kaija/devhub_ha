#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <bcm2835.h>
#include <unistd.h>
#include <curl/curl.h>

#define MAXTIMINGS 100

//#define DEBUG

#define DHT11 11
#define DHT22 22
#define AM2302 22

int get_if_addr(char *ifname, char *ip)
{
    int err_code = 0;
    if(!ifname || !ip) -1;

    int fd;
    struct ifreq ifr;
    memset(&ifr, 0x00, sizeof(struct ifreq));
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    err_code = ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    if(err_code < 0)
    {
        return -1;
    }
    else
    {
        sprintf(ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
        return 0;
    }
}

int report(char *api_key, char *project, int temp, int humd)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if(curl) {
		char buf[1024];
		sprintf(buf, "http://devicehub.net/io/%s/?apiKey=%s&Temperature=%d&Humidity=%d", project,api_key, temp, humd);
		printf("%s\n", buf);
    	curl_easy_setopt(curl, CURLOPT_URL, buf);
    	//curl_easy_setopt(curl, CURLOPT_POSTFIELDS, );

    	res = curl_easy_perform(curl);
    	if(res != CURLE_OK)
      		fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    	/* always cleanup */
    	curl_easy_cleanup(curl);
  	}
  	curl_global_cleanup();
  	return 0;
}


int readDHT(int type, int pin);

int main(int argc, char **argv)
{
  	if (!bcm2835_init())
        return 1;
	char ip[32];
  	while(1){
		if(get_if_addr("eth0", ip) == 0){
    		readDHT(DHT11, 4);
		}
    	sleep(60);
  	}
	return 0;
}

int test(int argc, char **argv){
  	if (!bcm2835_init())
        return 1;

  	if (argc != 3) {
		printf("usage: %s [11|22|2302] GPIOpin#\n", argv[0]);
		printf("example: %s 2302 4 - Read from an AM2302 connected to GPIO #4\n", argv[0]);
		return 2;
  	}
  	int type = 0;
  	if (strcmp(argv[1], "11") == 0) type = DHT11;
  	if (strcmp(argv[1], "22") == 0) type = DHT22;
  	if (strcmp(argv[1], "2302") == 0) type = AM2302;
  	if (type == 0) {
		printf("Select 11, 22, 2302 as type!\n");
		return 3;
  	}
  
  	int dhtpin = atoi(argv[2]);

  	if (dhtpin <= 0) {
		printf("Please select a valid GPIO pin #\n");
		return 3;
  	}


  	printf("Using pin #%d\n", dhtpin);
  	readDHT(type, dhtpin);
  	return 0;

} // main


int bits[MAXTIMINGS], data[100];
int bitidx = 0;

int readDHT(int type, int pin) {
	int counter = 0;
	int laststate = HIGH;
	int j=0;
	bitidx = 0;

	// Set GPIO pin to output
	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_write(pin, HIGH);
	usleep(500000);  // 500 ms
	bcm2835_gpio_write(pin, LOW);
	usleep(20000);

	bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);

	data[0] = data[1] = data[2] = data[3] = data[4] = 0;

	// wait for pin to drop?
	while (bcm2835_gpio_lev(pin) == 1) {
    	usleep(1);
  	}

  	// read data!
	int i = 0;
  	for (i=0; i< MAXTIMINGS; i++) {
    	counter = 0;
    	while ( bcm2835_gpio_lev(pin) == laststate) {
			counter++;
			//nanosleep(1);		// overclocking might change this?
        	if (counter == 1000)
	  			break;
    	}
    	laststate = bcm2835_gpio_lev(pin);
    	if (counter == 1000) break;
    	bits[bitidx++] = counter;

    	if ((i>3) && (i%2 == 0)) {
      		// shove each bit into the storage bytes
      		data[j/8] <<= 1;
      		if (counter > 200)
        		data[j/8] |= 1;
      		j++;
    	}
  	}


#ifdef DEBUG
  	for (int i=3; i<bitidx; i+=2) {
    	printf("bit %d: %d\n", i-3, bits[i]);
    	printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > 200);
  	}
#endif

  	printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);

  	if ((j >= 39) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) ) {
     	// yay!
     	if (type == DHT11){
			printf("Temp = %d *C, Hum = %d \%\n", data[2], data[0]);
			if(data[2] > 0 && data[2] < 100 && data[0] > 0 && data[0] < 100){
				report("2f4a689a-fb3a-4ffe-9551-bb576d9cea8b", "230", data[2], data[0]);
			}
		}
     	if (type == DHT22) {
			float f, h;
			h = data[0] * 256 + data[1];
			h /= 10;

			f = (data[2] & 0x7F)* 256 + data[3];
        	f /= 10.0;
        	if (data[2] & 0x80)  f *= -1;
			printf("Temp =  %.1f *C, Hum = %.1f \%\n", f, h);
    	}
    	return 1;
  	}

  	return 0;
}
