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
  
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))  
  
static void pabort(const char *s)  
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

static void transfer(int fd, char* tx, char* rx, int len)
{  
    int ret;
    struct spi_ioc_transfer tr[] = {  
        {
            .tx_buf = (unsigned long)tx,  
            .rx_buf = (unsigned long)rx,  
            .len = len,  
            .delay_usecs = delay,  
            .speed_hz = speed,  
            .bits_per_word = bits,  
        }
    };  
 
    clock_t start = clock();
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    sum_clock += clock() - start;
    if (ret < 1)
        pabort("can't send spi message");
}

// >>>>>>>>>> ================================================= //
// ============= intellino PARAMETERS & FUNCTIONS ============= //
// ============================================================ //
#define	VECTOR_LEN			64	// User define
#define	LEARN_COMMAND			0x60
#define	CLASSIFY_COMMAND		0x40
#define	READ_DISTANCE			0x83
#define	READ_CATEGORY			0x84
#define	DUMMY				0x00

// -------------------
// intellino LEARN
// -------------------
static void intellino_learn (int fd, int vector_length, char* learn_data, uint8_t learn_category)
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

	transfer(fd, learn_tx_buf, learn_rx_buf, vector_length+4);
}

// -------------------
// intellino CLASSIFY
// -------------------
static void intellino_classify (int fd, int vector_length, char* test_data, int *classified_distance, int *classified_category)
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

	transfer(fd, learn_tx_buf, learn_rx_buf, vector_length+11);

	*classified_distance = (learn_rx_buf[vector_length+5]<<8) + learn_rx_buf[vector_length+6];
	*classified_category = (learn_rx_buf[vector_length+9]<<8) + learn_rx_buf[vector_length+10];
}

// ------------------------
// intellino CLASSIFY_MULTI
// ------------------------
static void intellino_classify_multi (int fd, int multi_dataset_num, char (*test_multi_data)[VECTOR_LEN], int *classified_multi_distance, int *classified_multi_category)
{
	char learn_tx_buf[(VECTOR_LEN+11)*multi_dataset_num];
	char learn_rx_buf[(VECTOR_LEN+11)*multi_dataset_num];

	for (int j=0; j<multi_dataset_num; j++) {
		for (int i=0; i<VECTOR_LEN+3; i++) {
			switch(i) {
				case 0			:	learn_tx_buf[(VECTOR_LEN+11)*j+i] = CLASSIFY_COMMAND;
								break;
				case 1			:	learn_tx_buf[(VECTOR_LEN+11)*j+i] = (uint8_t)((VECTOR_LEN-1) >> 8);
								break;
				case 2			:	learn_tx_buf[(VECTOR_LEN+11)*j+i] = (uint8_t)((VECTOR_LEN-1) & 0x00FF);
								break;
				default			:	learn_tx_buf[(VECTOR_LEN+11)*j+i] = test_multi_data[j][i-3];
								break;
			}
		}
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+3)] = READ_DISTANCE;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+4)] = DUMMY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+5)] = DUMMY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+6)] = DUMMY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+7)] = READ_CATEGORY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+8)] = DUMMY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+9)] = DUMMY;
		learn_tx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+10)] = DUMMY;
	}

	transfer(fd, learn_tx_buf, learn_rx_buf, (VECTOR_LEN+11)*multi_dataset_num);

	for (int j=0; j<multi_dataset_num; j++) {
		classified_multi_distance[j] = (learn_rx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+5)]<<8) + learn_rx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+6)];
		classified_multi_category[j] = (learn_rx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+9)]<<8) + learn_rx_buf[(VECTOR_LEN+11)*j+(VECTOR_LEN+10)];
	}
}
// ============================================================ //
// ============= intellino PARAMETERS & FUNCTIONS ============= //
// ================================================= <<<<<<<<<< //

char multi_array_gen ();
  
  
int main()
{  
    int ret = 0;  
    int fd;  
  
  
    fd = open(device, O_RDWR);  
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
 
// >>>>>--------------
// Main operation
// -------------------
	int vector_length = 64;
	int dist;
	int cat;
	int multi_dataset_num = 50;
	int dist_multi[multi_dataset_num];
	int cat_multi[multi_dataset_num];

	/* DATASET */
	char data_001[64] = {1, 191, 128, 64, 2, 0, 191, 128, 128, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 127, 127, 128, 128, 0, 0, 2, 128, 127, 127, 1, 0, 1, 191, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 64, 191, 128, 0, 1, 1, 1, 1, 1, 1, 1};
	char data_002[64] = {128, 128, 128, 64, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 128, 65, 128, 127, 128, 64, 0, 1, 127, 128, 127, 65, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 128, 128, 1, 64, 64, 1, 1, 64, 128, 128, 65};
	char data_003[64] = {64, 191, 64, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 191, 127, 2, 2, 128, 192, 127, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 64, 191, 191, 127, 1, 2, 128, 64, 2, 0};
	char data_004[64] = {127, 65, 128, 191, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 65, 1, 2, 0, 127, 128, 0, 2, 128, 128, 2, 1, 128, 191, 127, 64, 0, 64, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 65, 128, 191, 128, 1, 1, 1, 1, 1, 1, 127, 192};
	char data_005[64] = {65, 127, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 129, 64, 127, 128, 127, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 65, 127, 128, 65, 1, 1, 1, 1};
	char data_006[64] = {128, 64, 127, 128, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 128, 128, 1, 0, 128, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 64, 1, 1, 128, 1, 128, 128};
	char data_007[64] = {128, 128, 191, 127, 128, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 128, 128, 191, 128, 64, 65, 0, 0, 0, 1, 191, 127, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 191, 128, 128, 0, 1, 1, 64, 191, 65, 127, 128, 65};
	char data_008[64] = {65, 1, 1, 1, 65, 128, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 64, 1, 1, 191, 128, 128, 128, 63, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 127, 128, 64, 2, 0, 1, 128};
	char data_009[64] = {1, 128, 127, 128, 1, 1, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 128, 1, 1, 1, 0, 0, 1, 128, 127, 128, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 64, 128, 128, 1, 0, 0, 0, 1, 128, 65, 1, 0};
	char data_010[64] = {128, 128, 128, 128, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 64, 128, 64, 64, 1, 0, 0, 127, 128, 191, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 128, 127, 128, 128, 64, 1, 1, 2, 65, 127, 128};
	char data_011[64] = {0, 0, 191, 127, 65, 128, 127, 128, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 128, 64, 1, 1, 0, 0, 0, 1, 64, 1, 0, 1, 1, 64, 127, 191, 64, 0, 1, 1, 0, 128, 128, 190, 64, 0, 0, 0, 0, 0, 64, 128, 190, 63, 1, 1, 64, 64, 0, 0, 0, 1, 0, 1, 0};
	char data_012[64] = {127, 1, 1, 64, 127, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 127, 128, 128, 128, 64, 191, 64, 1, 1, 128, 128, 191, 65, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 2, 1, 65, 190, 127, 1, 1, 1, 2, 1, 0, 64, 128, 192, 64, 1, 2};
	char data_013[64] = {0, 64, 1, 127, 127, 1, 0, 65, 1, 1, 64, 1, 2, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 128, 127, 127, 65, 1, 64, 191, 2, 191, 127, 2, 0, 191, 64, 127, 64, 64, 191, 127, 1, 2, 0, 2, 0, 1, 65, 191, 128, 191, 64, 1, 191, 64, 191, 1, 1, 128, 191, 1, 190, 64};
	char data_014[64] = {0, 1, 64, 128, 191, 1, 1, 1, 0, 1, 2, 1, 1, 1, 0, 1, 64, 191, 64, 1, 1, 1, 64, 128, 1, 1, 0, 1, 0, 192, 0, 1, 1, 128, 64, 128, 191, 1, 1, 0, 1, 1, 1, 1, 1, 128, 129, 0, 2, 2, 65, 191, 65, 127, 128, 127, 0, 2, 1, 1, 128, 126, 127, 1};
	char data_015[64] = {64, 64, 191, 128, 1, 2, 1, 0, 0, 0, 0, 0, 1, 254, 1, 128, 128, 64, 128, 191, 127, 64, 64, 1, 127, 128, 0, 127, 127, 192, 127, 1, 0, 64, 128, 0, 0, 0, 0, 0, 0, 1, 0, 128, 128, 1, 127, 1, 0, 0, 65, 191, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 1, 1};
	char data_016[64] = {128, 128, 64, 1, 1, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 128, 127, 128, 127, 1, 3, 64, 0, 1, 127, 127, 0, 1, 64, 191, 128, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 64, 191, 65, 1, 0, 0, 65, 128, 127, 128, 128, 1, 127, 65, 64, 1, 1};
	char data_017[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	char data_018[64] = {1, 0, 65, 191, 1, 1, 1, 0, 1, 1, 1, 2, 1, 0, 1, 2, 1, 128, 191, 1, 1, 1, 1, 64, 1, 1, 0, 64, 191, 128, 1, 1, 64, 128, 191, 64, 1, 1, 1, 65, 0, 1, 0, 1, 1, 64, 191, 65, 64, 127, 64, 190, 128, 128, 128, 127, 1, 1, 1, 64, 128, 65, 127, 64};
	char data_019[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	char data_020[64] = {128, 64, 65, 128, 190, 65, 2, 2, 1, 0, 1, 0, 0, 0, 0, 1, 1, 64, 191, 64, 1, 0, 1, 1, 1, 1, 127, 128, 127, 1, 191, 1, 64, 0, 1, 128, 191, 128, 64, 0, 1, 0, 0, 0, 0, 128, 127, 191, 1, 128, 128, 128, 191, 128, 128, 128, 1, 0, 1, 65, 65, 127, 128, 128};
	char data_021[64] = {2, 2, 128, 1, 1, 0, 1, 1, 2, 1, 64, 64, 64, 191, 128, 128, 127, 1, 64, 2, 1, 0, 0, 1, 0, 1, 0, 1, 64, 128, 64, 1, 128, 128, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 2, 0, 1, 1, 1, 1, 0, 1, 1, 1, 2, 64, 191, 0, 64};
	char data_022[64] = {0, 1, 65, 128, 128, 127, 191, 127, 127, 127, 65, 0, 0, 191, 65, 0, 1, 0, 1, 1, 191, 128, 127, 1, 0, 1, 0, 1, 0, 1, 0, 128, 127, 127, 1, 0, 1, 0, 1, 0, 1, 1, 1, 128, 128, 128, 128, 128, 128, 191, 127, 1, 1, 0, 1, 2, 0, 0, 0, 0, 1, 1, 0, 64};
	char data_023[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	char data_024[64] = {127, 128, 191, 64, 0, 1, 2, 0, 0, 0, 0, 1, 128, 129, 0, 1, 0, 191, 127, 127, 0, 128, 0, 1, 127, 1, 127, 128, 191, 128, 128, 64, 1, 127, 191, 127, 64, 0, 0, 0, 0, 0, 128, 1, 128, 63, 128, 64, 1, 127, 128, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 128, 127, 65};
	char data_025[64] = {0, 0, 0, 1, 2, 0, 1, 1, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 128, 0, 0, 1, 1, 1, 64, 191, 64, 128, 127, 64, 64, 0, 64, 128, 64, 64, 127, 191};
	char data_026[64] = {2, 1, 2, 127, 1, 190, 64, 128, 191, 127, 64, 128, 127, 129, 64, 0, 128, 128, 128, 127, 64, 127, 191, 128, 1, 1, 64, 1, 64, 191, 65, 128, 128, 1, 1, 1, 191, 1, 1, 191, 128, 128, 64, 127, 1, 0, 65, 192, 64, 64, 128, 127, 191, 128, 1, 191, 1, 1, 64, 191, 190, 0, 128, 64};
	char data_027[64] = {65, 191, 65, 0, 1, 127, 1, 1, 0, 1, 1, 191, 1, 191, 65, 1, 127, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 64, 191, 1, 64, 128, 127, 1, 1, 127, 1, 1, 1, 64, 128, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 2, 0, 1, 0, 1, 128, 128, 0, 1, 0};
	char data_028[64] = {1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 128, 128, 64, 1, 1, 1, 2, 1, 1, 1, 65, 2, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 1, 128, 128, 0, 1, 65, 191, 128, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0};
	char data_029[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	char data_030[64] = {128, 191, 1, 1, 191, 128, 1, 0, 0, 0, 0, 1, 1, 128, 127, 128, 128, 1, 0, 0, 1, 1, 1, 2, 0, 1, 64, 64, 1, 1, 1, 64, 128, 64, 128, 64, 128, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 1, 0, 1, 1, 1, 1, 64, 127, 1, 1, 1, 1, 1, 1, 1};
	char data_150[64] = {1, 1, 1, 1, 1, 1, 1, 1, 127, 128, 64, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 128, 1, 1, 0, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 0, 1, 2, 0, 192, 64, 0, 0, 0, 0, 0, 0, 1, 0, 129, 127, 191, 127, 1, 1, 2, 1, 0, 1, 1, 1};

	char classify_multi_data[50][64] = {
	// data_101[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_102[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_103[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_104[64]
		{1, 64, 1, 1, 128, 64, 128, 64, 64, 1, 1, 2, 1, 0, 1, 1, 1, 0, 1, 1, 2, 0, 128, 65, 128, 128, 1, 64, 0, 127, 128, 1, 127, 191, 1, 0, 190, 65, 127, 129, 65, 191, 127, 1, 1, 1, 1, 64, 127, 128, 128, 192, 127, 128, 191, 64, 128, 1, 192, 64, 128, 191, 0, 192},
	// data_105[64]
		{1, 0, 1, 1, 64, 128, 127, 64, 0, 1, 0, 1, 1, 1, 0, 2, 1, 0, 1, 1, 2, 1, 64, 127, 1, 0, 0, 0, 64, 0, 0, 0, 1, 2, 0, 1, 1, 1, 1, 128, 191, 64, 2, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 65, 191, 1, 1, 1, 0, 0, 1, 0, 1, 0},
	// data_106[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_107[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_108[64]
		{64, 1, 65, 191, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 127, 64, 1, 0, 191, 1, 2, 2, 65, 191, 1, 1, 1, 128, 191, 128, 1, 0, 2, 1, 65, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 255, 0, 64, 64, 191, 64, 64, 191, 0, 128},
	// data_109[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_110[64]
		{128, 1, 0, 1, 0, 0, 64, 128, 127, 64, 64, 64, 1, 1, 2, 1, 1, 1, 1, 64, 65, 127, 128, 64, 1, 0, 0, 0, 1, 1, 0, 1, 127, 191, 191, 64, 0, 1, 0, 1, 2, 1, 0, 65, 127, 128, 128, 127, 128, 127, 127, 128, 1, 2, 0, 64, 0, 0, 0, 0, 0, 1, 1, 1},
	// data_111[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_112[64]
		{64, 1, 1, 1, 1, 191, 1, 64, 191, 1, 64, 191, 128, 127, 128, 64, 64, 0, 64, 0, 1, 1, 64, 190, 1, 0, 1, 191, 128, 0, 64, 64, 1, 1, 2, 1, 0, 64, 191, 128, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 128, 128, 191, 64, 65, 1, 1, 1, 1, 64, 127, 1},
	// data_113[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_114[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_115[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_116[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_117[64]
		{1, 1, 127, 128, 0, 65, 128, 127, 65, 1, 65, 0, 1, 1, 1, 65, 127, 127, 127, 65, 0, 1, 127, 64, 128, 64, 1, 128, 191, 1, 192, 65, 1, 2, 191, 1, 65, 0, 128, 128, 128, 191, 1, 1, 1, 64, 191, 128, 64, 127, 65, 191, 2, 191, 64, 127, 2, 0, 191, 1, 0, 64, 128, 1},
	// data_118[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_119[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_120[64]
		{128, 128, 1, 128, 64, 127, 64, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 64, 128, 64, 64, 1, 64, 191, 1, 1, 64, 191, 65, 0, 1, 65, 64, 191, 128, 1, 1, 1, 0, 2, 0, 1, 0, 1, 65, 1, 1, 1, 1, 128, 1, 191, 128, 127, 1, 64, 1, 1, 191, 1},
	// data_121[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_122[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_123[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_124[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_125[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_126[64]
		{1, 0, 65, 128, 0, 0, 0, 0, 0, 64, 191, 191, 128, 191, 64, 128, 1, 1, 64, 1, 64, 1, 1, 192, 0, 65, 64, 192, 64, 191, 1, 2, 0, 2, 1, 127, 129, 128, 0, 1, 0, 65, 1, 1, 1, 0, 64, 1, 1, 1, 0, 0, 1, 1, 64, 64, 1, 2, 1, 1, 190, 1, 0, 1},
	// data_127[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_128[64]
		{0, 1, 0, 0, 1, 0, 2, 1, 1, 0, 192, 65, 1, 0, 0, 0, 0, 0, 0, 1, 1, 127, 191, 1, 0, 0, 1, 0, 0, 0, 0, 1, 2, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 65, 127, 128, 127, 0, 1, 64, 128, 128, 128, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},
	// data_129[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_130[64]
		{64, 128, 1, 0, 191, 1, 127, 192, 127, 191, 127, 63, 128, 128, 128, 65, 0, 127, 127, 128, 128, 0, 191, 64, 127, 1, 1, 2, 0, 191, 65, 65, 191, 0, 1, 1, 65, 191, 1, 64, 129, 0, 2, 191, 0, 64, 64, 191, 128, 64, 64, 1, 64, 128, 1, 1, 128, 1, 128, 127, 127, 64, 64, 191},
	// data_131[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_132[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_133[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_134[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_135[64]
		{1, 0, 1, 0, 1, 1, 128, 191, 128, 1, 1, 1, 2, 1, 1, 0, 2, 1, 0, 64, 65, 63, 191, 1, 1, 1, 1, 1, 2, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 64, 127, 1, 1, 1, 0, 0, 1, 1, 1, 1, 64, 63, 66, 1, 1, 1, 1, 2, 1, 1, 0, 0, 0},
	// data_136[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_137[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_138[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_139[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_140[64]
		{1, 1, 1, 1, 2, 127, 65, 0, 0, 1, 0, 64, 128, 65, 127, 1, 0, 1, 1, 1, 1, 1, 0, 1, 127, 0, 0, 1, 1, 1, 191, 1, 2, 2, 1, 1, 64, 64, 127, 128, 2, 1, 0, 2, 0, 0, 0, 0, 0, 1, 0, 2, 1, 1, 1, 64, 65, 0, 2, 1, 1, 1, 65, 1},
	// data_141[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_142[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_143[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_144[64]
		{0, 0, 1, 0, 1, 0, 1, 1, 2, 64, 127, 1, 0, 1, 0, 0, 128, 128, 127, 127, 128, 128, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 1, 65, 0, 127, 128, 128, 128, 65, 65, 0, 1, 1, 1, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0},
	// data_145[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_146[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_147[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_148[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_149[64]
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	// data_150[64]
		{1, 1, 1, 1, 1, 1, 1, 1, 127, 128, 64, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 128, 1, 1, 0, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 0, 1, 2, 0, 192, 64, 0, 0, 0, 0, 0, 0, 1, 0, 129, 127, 191, 127, 1, 1, 2, 1, 0, 1, 1, 1}
	};

	// Learning
	printf("Learning...\n");
	intellino_learn(fd, vector_length, data_001, 1);
	intellino_learn(fd, vector_length, data_002, 2);
	intellino_learn(fd, vector_length, data_003, 3);
	intellino_learn(fd, vector_length, data_004, 4);
	intellino_learn(fd, vector_length, data_005, 5);
	intellino_learn(fd, vector_length, data_006, 6);
	intellino_learn(fd, vector_length, data_007, 7);
	intellino_learn(fd, vector_length, data_008, 8);
	intellino_learn(fd, vector_length, data_009, 9);
	intellino_learn(fd, vector_length, data_010, 10);
	intellino_learn(fd, vector_length, data_011, 11);
	intellino_learn(fd, vector_length, data_012, 12);
	intellino_learn(fd, vector_length, data_013, 13);
	intellino_learn(fd, vector_length, data_014, 14);
	intellino_learn(fd, vector_length, data_015, 15);
	intellino_learn(fd, vector_length, data_016, 16);
	intellino_learn(fd, vector_length, data_017, 17);
	intellino_learn(fd, vector_length, data_018, 18);
	intellino_learn(fd, vector_length, data_019, 19);
	intellino_learn(fd, vector_length, data_020, 20);
	intellino_learn(fd, vector_length, data_021, 21);
	intellino_learn(fd, vector_length, data_022, 22);
	intellino_learn(fd, vector_length, data_023, 23);
	intellino_learn(fd, vector_length, data_024, 24);
	intellino_learn(fd, vector_length, data_025, 25);
	intellino_learn(fd, vector_length, data_026, 26);
	intellino_learn(fd, vector_length, data_027, 27);
	intellino_learn(fd, vector_length, data_028, 28);
	intellino_learn(fd, vector_length, data_029, 29);
	intellino_learn(fd, vector_length, data_030, 30);
	printf("Done\n");

	// Multi classification
	printf("Multi Classification...\n");
	intellino_classify_multi(fd, multi_dataset_num, classify_multi_data, dist_multi, cat_multi);
	for (int j=0; j<multi_dataset_num; j++) {
		printf("data_%d -> DIST : %d / CAT : %d\n", (j+101), dist_multi[j], cat_multi[j]);
	}
	printf("Done\n");

	// Single classification
	printf("Single Classification...\n");
	intellino_classify(fd, vector_length, data_150, &dist, &cat);
	printf("data_150 -> DIST : %d / CAT : %d\n", dist, cat);
	printf("Done\n");



    puts("");
    printf("spi transfer delay time (micro second) : %f\n", (double)sum_clock / CLOCKS_PER_SEC *1000 * 1000);

    close(fd);
    return ret;  
}
