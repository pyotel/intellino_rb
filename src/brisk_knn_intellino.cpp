#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./intellino_spi.h"

using namespace std;

Intellino_spi manager;
char buffer[512];
char tokenizer [] = ",\t\n\0";
static const int vector_max_len =  Intellino_spi::vector_max_len;
static const int min_vector_len =  5;
char vector_char[vector_max_len];

static const int test_num = 54;
static const int vectors_num = 54;

int train_intellino(const char* input_train_file, int sample_num, bool debug_print){    
    FILE *fp = fopen(input_train_file, "r");
    if(fp == NULL){
        return -1;
        printf("File not found!!!\n");
    }
    int cat = 1;
    while(!feof(fp)){
        fgets(buffer, sizeof(buffer), fp);
        char* tok_buffer = (char*)strtok(buffer, tokenizer);
        int vector_index = 0;
        while(tok_buffer != NULL){
            vector_char[vector_index] = (char)atoi(tok_buffer);
            vector_index++;
            tok_buffer = strtok(NULL, tokenizer);            
        }
        if(vector_index < min_vector_len) continue;

        manager.learn(vector_index, vector_char, cat);

        if(debug_print){
            printf("%d VECTOR : ", cat);
            for(int i =0; i < vector_index ; i++) printf("%d, ",vector_char[i]);
            putchar('\n');
            putchar('\n');
        }

        for(int i =0; i < vector_max_len ; i++) vector_char[i] = 0;

        cat++;
        if(sample_num > 0 && cat == sample_num + 1) break; // for dubug
    }
    fclose(fp);
    return 0;
}

int test_intellino(){
    FILE *fp = fopen("../data/train_img.csv", "r");
    if(fp == NULL){
        return -1;
        printf("File not found!!!\n");
    }
    int cat = 1;
    int ret_dist =0, ret_cat=0;
    while(!feof(fp)){
        fgets(buffer, sizeof(buffer), fp);
        char* tok_buffer = (char*)strtok(buffer, tokenizer);
        int vector_index = 0;
        while(tok_buffer != NULL){
            vector_char[vector_index] = (char)atoi(tok_buffer) + 10;
            vector_index++;
            tok_buffer = strtok(NULL, tokenizer);
        }
        if(vector_index < min_vector_len) continue;

        manager.classify(vector_index, vector_char, &ret_dist, &ret_cat);
        
        if(true){
            printf("VECTOR : ");
            for(int i =0; i < vector_index ; i++) printf("%d, ",vector_char[i]);
            putchar('\n');
            printf("Expected Cat : %d, Distance : %d, Category : %d\n", cat, ret_dist, ret_cat);
        }        

        for(int i =0; i < vector_max_len ; i++) vector_char[i] = 0;
        cat++;

        if(cat == test_num) break; // for dubug
    }
    fclose(fp);
    return 0;
}

int test_multi(const char* input_test_file, int sample_num, bool debug_print){
    FILE *fp = fopen(input_test_file, "r");
    if(fp == NULL){
        return -1;
        printf("File not found!!!\n");
    }
    int line_num = 1;
    int ret_dist[vector_max_len];
    int ret_cat[vector_max_len];
    int vectors_id = 0;
    char vectors[vectors_num][vector_max_len] = {0};
    char vector_buffer[vector_max_len] = {0};
    while(!feof(fp)){
        fgets(buffer, sizeof(buffer), fp);
        char* tok_buffer = (char*)strtok(buffer, tokenizer);
        int vector_index = 0;
        while(tok_buffer != NULL){
            vectors[vectors_id][vector_index]  = (char)atoi(tok_buffer) + 5;
            vector_index++;
            tok_buffer = strtok(NULL, tokenizer);
        }
        if(vector_index < min_vector_len){
            printf("[WARNING] line number %d does not have more %d elements\n", line_num, min_vector_len);
            break;
        }
        
        vectors_id++;
        if(vectors_id == vectors_num){
            vectors_id = 0;
            manager.classify_multi(vectors_num, vector_index, vectors, ret_dist, ret_cat);            
            if(debug_print){
                for(int i=0; i < vectors_num ; i++){
                    if(i+1 != ret_cat[i]){
                        printf("VECTOR : ");
                        for(int j =0; j < vector_index; j++) printf("%d, ",vectors[i][j]);
                        putchar('\n');
                        printf("Expected Cat : %d, Distance : %d, Category : %d\n", i+1, ret_dist[i], ret_cat[i]);
                        putchar('\n');
                    }
                }
            }
        }
        vector_index = 0;
        line_num++;
        if(sample_num > 0 && line_num >= sample_num + 1) break; // for dubug
    }
    fclose(fp);
    return 0;
}

int main(){
    train_intellino("../data/train_img.csv", -1, false);
    puts("Training is finished.");

    for(int i=0; i < 5; i++) test_multi("../data/train_img.csv", test_num, true);
    puts("Partial Sample Multi Testing is finished.");

    test_multi("../data/test_img.csv", -1, false);
    puts("All Sample Multi Testing is finished.");

    return 0;
}