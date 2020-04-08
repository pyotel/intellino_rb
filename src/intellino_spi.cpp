#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <stdarg.h>
#include "intellino_spi.h"  


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))  

  
void pabort(const char *s)  
{  
    perror(s);  
    abort();  
}  
  
static const char *device = "/dev/spidev0.0";  
static uint8_t mode = SPI_CPHA | SPI_CPOL; //SPI_MODE3
static uint8_t bits = 8;  
static uint32_t speed = 8000000;   // max speed = 8000000
static uint16_t delay = 0;
clock_t sum_clock = 0;

void transfer(int fd, char* tx, char* rx, int len)
{  
    int ret;
    struct spi_ioc_transfer tr[] = {  
        {
            .tx_buf = (unsigned long)tx,  
            .rx_buf = (unsigned long)rx,  
            .len = (__u32)len,  
            .delay_usecs = delay,  
            //.speed_hz = speed,  
            .bits_per_word = bits,  
        }
    };
	tr[0].speed_hz = speed;
 
    clock_t start = clock();
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    sum_clock += clock() - start;
    if (ret < 1)
        pabort("can't send spi message");
}

// >>>>>>>>>> ================================================= //
// ============= intellino PARAMETERS & FUNCTIONS ============= //
// ============================================================ //
#define	LEARN_COMMAND			0x60
#define	CLASSIFY_COMMAND		0x40
#define	READ_DISTANCE			0x83
#define	READ_CATEGORY			0x84
#define	DUMMY				0x00

Intellino_spi::Intellino_spi(){
	int ret = 0;

    int fd = open(device, O_RDWR);  
	this->spi_fd = fd;
    if (fd < 0)  
        pabort("can't open device");  
  
    /* 
     * spi mode 
     */  
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);  
    if (ret == -1)  
        pabort("can't set spi mode");  
  
    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);  
    if (ret == -1)  
        pabort("can't get spi mode");  
  
    /* 
     * bits per word 
     */  
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);  
    if (ret == -1)  
        pabort("can't set bits per word");  
  
    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);  
    if (ret == -1)  
        pabort("can't get bits per word");  
  
    /* 
     * max speed hz 
     */  
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);  
    if (ret == -1)  
        pabort("can't set max speed hz");  
  
    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);  
    if (ret == -1)  
        pabort("can't get max speed hz");  
  
    printf("spi mode: %d\n", mode);  
    printf("bits per word: %d\n", bits);  
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);  
}


// -------------------
// intellino LEARN
// -------------------
void Intellino_spi::learn (int vector_length, char* learn_data, uint8_t learn_category)
{
	char learn_tx_buf[vector_length+4];
	char learn_rx_buf[vector_length+4];

	for (int i=0; i<vector_length+3; i++) {
		switch(i) {
			case 0			:	learn_tx_buf[i] = LEARN_COMMAND;
							break;
			case 1			:	learn_tx_buf[i] = (uint8_t)((vector_length-1) >> 8);
							break;
			case 2			:	learn_tx_buf[i] = (uint8_t)((vector_length-1) & 0x00FF);
							break;
			default			:	learn_tx_buf[i] = learn_data[i-3];
							break;
		}
	}
	learn_tx_buf[vector_length+3] = learn_category;

	transfer(this->spi_fd, learn_tx_buf, learn_rx_buf, vector_length+4);
}

// -------------------
// intellino CLASSIFY
// -------------------
void Intellino_spi::classify (int vector_length, char* test_data, int *classified_distance, int *classified_category)
{
	char learn_tx_buf[vector_length+11];
	char learn_rx_buf[vector_length+11];

	for (int i=0; i<vector_length+3; i++) {
		switch(i) {
			case 0			:	learn_tx_buf[i] = CLASSIFY_COMMAND;
							break;
			case 1			:	learn_tx_buf[i] = (uint8_t)((vector_length-1) >> 8);
							break;
			case 2			:	learn_tx_buf[i] = (uint8_t)((vector_length-1) & 0x00FF);
							break;
			default			:	learn_tx_buf[i] = test_data[i-3];
							break;
		}
	}
	learn_tx_buf[vector_length+3] = READ_DISTANCE;
	learn_tx_buf[vector_length+4] = DUMMY;
	learn_tx_buf[vector_length+5] = DUMMY;
	learn_tx_buf[vector_length+6] = DUMMY;
	learn_tx_buf[vector_length+7] = READ_CATEGORY;
	learn_tx_buf[vector_length+8] = DUMMY;
	learn_tx_buf[vector_length+9] = DUMMY;
	learn_tx_buf[vector_length+10] = DUMMY;

	transfer(this->spi_fd, learn_tx_buf, learn_rx_buf, vector_length+11);

	*classified_distance = (learn_rx_buf[vector_length+5]<<8) + learn_rx_buf[vector_length+6];
	*classified_category = (learn_rx_buf[vector_length+9]<<8) + learn_rx_buf[vector_length+10];
}

// ------------------------
// intellino CLASSIFY_MULTI
// ------------------------
void Intellino_spi::classify_multi (int multi_dataset_num, int vector_length,
					char test_multi_data[][vector_max_len], int *classified_multi_distance, int *classified_multi_category)
{
	char learn_tx_buf[(vector_length+11)*multi_dataset_num];
	char learn_rx_buf[(vector_length+11)*multi_dataset_num];

	for (int j=0; j<multi_dataset_num; j++) {
		for (int i=0; i<vector_length+3; i++) {
			switch(i) {
				case 0			:	learn_tx_buf[(vector_length+11)*j+i] = CLASSIFY_COMMAND;
								break;
				case 1			:	learn_tx_buf[(vector_length+11)*j+i] = (uint8_t)((vector_length-1) >> 8);
								break;
				case 2			:	learn_tx_buf[(vector_length+11)*j+i] = (uint8_t)((vector_length-1) & 0x00FF);
								break;
				default			:	learn_tx_buf[(vector_length+11)*j+i] = test_multi_data[j][i-3];
								break;
			}
		}
		learn_tx_buf[(vector_length+11)*j+(vector_length+3)] = READ_DISTANCE;
		learn_tx_buf[(vector_length+11)*j+(vector_length+4)] = DUMMY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+5)] = DUMMY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+6)] = DUMMY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+7)] = READ_CATEGORY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+8)] = DUMMY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+9)] = DUMMY;
		learn_tx_buf[(vector_length+11)*j+(vector_length+10)] = DUMMY;
	}

	transfer(this->spi_fd, learn_tx_buf, learn_rx_buf, (vector_length+11)*multi_dataset_num);

	for (int j=0; j<multi_dataset_num; j++) {
		classified_multi_distance[j] = (learn_rx_buf[(vector_length+11)*j+(vector_length+5)]<<8) + learn_rx_buf[(vector_length+11)*j+(vector_length+6)];
		classified_multi_category[j] = (learn_rx_buf[(vector_length+11)*j+(vector_length+9)]<<8) + learn_rx_buf[(vector_length+11)*j+(vector_length+10)];
	}
}