#include <stdint.h>
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0])) 

class Intellino_spi{
private:
    int spi_fd = 0;

public:
    static const int vector_max_len = 64;
    Intellino_spi();
    void learn (int vector_length, char* learn_data, uint8_t learn_category);
    void classify (int vector_length, char* test_data, int *classified_distance, int *classified_category);
    void classify_multi (int multi_dataset_num, int vector_length,
                char test_multi_data[][vector_max_len], int *classified_multi_distance, int *classified_multi_category);
};