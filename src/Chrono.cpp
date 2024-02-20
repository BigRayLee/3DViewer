#include <sys/time.h>
#include <stdio.h>
#include <ctime>

#include "Chrono.h"

struct timeval tv0, tv1;

void TimerStart(){
	gettimeofday(&tv0, NULL);
}

unsigned TimerStop(const char *str){
	float elapsedTime = 0.0;
	gettimeofday(&tv1, NULL);
	unsigned int mus = 1000000 * (tv1.tv_sec - tv0.tv_sec);
	mus += (tv1.tv_usec - tv0.tv_usec);
	if (str[0]){
		printf("%s: ", str);
		if  (mus >= 1000000) {
			printf("%.3f s \n", (float)mus / 1000000);
		}
		else {	
			printf("%.3f ms \n", (float)mus / 1000);
		}
	}
	return (mus);
}

char* GetCurrentTime(){
	time_t now = time(0);

    char* dt = ctime(&now);
	return dt;
}

void GetElapsedTime(timeval start, timeval end, const char* str ){
	unsigned int mus = 1000000 * (end.tv_sec - start.tv_sec);
	mus += (end.tv_usec - start.tv_usec);
	printf("%s: ", str);
    if  (mus >= 1000000) {
        printf(" %.3f s ", (float)mus / 1000000);
    }
    else {
        printf(" %.3f ms ", (float)mus / 1000);
    }
}