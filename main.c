#include "unicode/ubrk.h"
#include "unicode/ucnv.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/ustdio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1000 // Buffer is a max possible number of UChar per line in file
#define MONTE_CARLO 100 // Number of monte carlo simulation to perform

/**
  * Load UTF-8 dictionary file from `fp` into `result` and return pointer `tokens` which 
  * contains an index and length of each token in `result`. It return `result_len` which is
  * boundary of `result` and `tokens_len` which is boundary of `tokens`.
  * 
  * The `tokens` pointer has following layout:
  * -----------------------------------------------------------------------------------------------------------
  * | offset of token 0 in result | length of token 0 in result | ... | offset of token n | lenght of token n |
  * -----------------------------------------------------------------------------------------------------------
  */
void load_dict(UFILE* fp, UChar* result, int32_t* tokens, size_t* result_len, size_t* tokens_len) {
    UChar * line = malloc(BUFFER_SIZE * sizeof(UChar));
    size_t index = 0;
    size_t i = 0;

    if (fp == NULL)
        return;

    UChar* new_line = malloc(2 * sizeof(UChar));
    UErrorCode err = U_ZERO_ERROR;
    u_strFromUTF8(new_line, 2 * sizeof(UChar), NULL, "\r\n", 2, &err);

    if (U_FAILURE(err)) {
        printf("Fail to make 'new_line' UChar");
    }

    while (u_fgets(line, BUFFER_SIZE, fp) != NULL) {
        line[u_strcspn(line, new_line)] = 0;
        int read = u_strlen(line);
        tokens[i] = index;
        tokens[i + 1] = read; // store as length
        memcpy(&result[index], line, read * sizeof(UChar)); 
        index += read;
        i += 2;
    }

    free(new_line);
    free(line);
    result[index + 1] = 0; // append null to terminate the result
    *tokens_len = i;
    *result_len = index + 1;
}

// get file size in byte
int get_size(char* path) {
    FILE* f = fopen("data/lexitron_mod.txt", "r");
    fseek(f, 0L, SEEK_END);
    int sz = ftell(f);
    fclose(f);
    return sz;
}

// A utility function to swap to integers 
void swap (int32_t* a, int32_t* b) { 
    int32_t temp = *a; 
    *a = *b; 
    *b = temp; 
    temp = a[1];
    a[1] = b[1];
    b[1] = temp;
} 

// A function to generate a random permutation of arr[] 
void randomize ( int32_t* arr, size_t n ) { 
    // Use a different seed value so that we don't get same 
    // result each time we run this program 
    srand ( time(NULL) ); 
  
    // Start from the last element and swap one by one. We don't 
    // need to run for the first element that's why i > 0 
    for (int i = n / 2; i > 0; i--) { 
        // Pick a random index from 0 to i 
        int j = rand() % (i + 1); 
  
        // Swap arr[i] with the element at random index 
        swap((int*) &arr[i * 2], (int*) &arr[j * 2]); 
    } 
} 

/**
  * Create a new text from current tokens pointer.
  * 
  * This is utility function that will be used after each time the tokens got shuffle.
  * It will use `raw` as source and `result` to return a new text.
  */
void make_text(int32_t* tokens, size_t tokens_len, UChar* raw, UChar* result) {
    size_t offset = 0;
    for (size_t i = 0; i < tokens_len; i += 2) {
        memcpy(&result[offset], &raw[tokens[i]], tokens[i + 1] * sizeof(UChar));
        offset += tokens[i + 1];
    }
    result[offset + 1] = 0; // null terminate the result
}

/**
  * Verify break iterator word tokenizer result against give `tokens` pointer.
  * 
  * See `load_dict` function for layout of this `tokens` pointer.
  * It return `true_positive` and `predicted_positive` value through given parameter.
  */
void verify_tokens(UBreakIterator* bi, int32_t* tokens, size_t tokens_len, int32_t* true_positive, int32_t* predicted_positive) {
    int32_t prev = 0;
    int32_t next = ubrk_next(bi);
    int32_t i = 1; // Odd number index point to length of each tokens in pointer
    int32_t expected_offset = 0; // offset of shuffled text
    int32_t expected_bound = expected_offset + tokens[i]; // bound of shuffled text

    do {
        // printf("%d %d %d %d\n",prev, expected_offset, next, expected_bound);
        if (prev == expected_offset && next == expected_bound) {
            // exactly the same slice
            prev = next;
            next = ubrk_next(bi);
            expected_offset = expected_bound;
            i += 2; // Tokens pointer need to be incremented by 2 as it store pair of value
            (*true_positive)++;

            if (next != UBRK_DONE) {
                (*predicted_positive)++;
            } else {
                break;
            }

            expected_bound = expected_offset + tokens[i]; // i is shifted so we have new bound
        } else if (next < expected_bound) {
            // Break iterator return shorter token than expected text
            // Need to continue move break iterator to next token to match with expected token.
            prev = next;
            next = ubrk_next(bi);

            if (next != UBRK_DONE) {
                (*predicted_positive)++;
            } else {
                break;
            }
        } else if (next >= expected_bound) {
            // Break iterator advance through the span of current expected token
            // Need to move expected token to match with current break iterator position.

            expected_offset = expected_bound; // move offset by current bound
            i += 2; // Tokens pointer need to be incremented by 2 as it store pair of value
            expected_bound = expected_offset + tokens[i]; // new bound equals to length diff 
        }
    } while(i < tokens_len - 1);
}

void monte_carlo_sim(UChar* source, size_t source_len, int32_t* tokens, size_t tokens_len) {
    UChar* shuffled = malloc(source_len * sizeof(UChar));
    UErrorCode err = U_ZERO_ERROR;
    // make an iterator that will be reused for entire simulation
    UBreakIterator* bi = ubrk_open(UBRK_WORD, "th_TH", NULL, -1, &err);

    if U_FAILURE(err) {
        printf("Fail to instantiate ICU Word break iterator with following error code: %d", err);
        return;
    }
    
    // metric to be measure
    double best_f1 = 0;
    double worst_f1 = 1;
    double mean_f1 = 0;
    double mean_tokenization = 0;
    double var_f1 = 0;
    double var_tokenization = 0;

    for (int sim = 0; sim < MONTE_CARLO; sim++) {
        printf("Begin simulation %d\n", sim);
        randomize(tokens, tokens_len); // shuffle tokens
        make_text(tokens, tokens_len, source, shuffled); // make a text from shuffled token
        err = U_ZERO_ERROR;
        struct timespec start, end;
        timespec_get(&start, TIME_UTC);
        ubrk_setText(bi, shuffled, source_len, &err); // set shuffled text to break iterator
        timespec_get(&end, TIME_UTC);
        uint64_t delta_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
        printf("Simulation %d total time to setText in %llu ms\n", sim, delta_ms);

        if U_FAILURE(err) {
            printf("Fail to setText on UBreakIterator in simulation %d", sim);
            return;
        }

        // current simulation metric
        int32_t true_positive = 0;
        int32_t predicted_positive = 0;
        int32_t expected_positive = tokens_len;

        timespec_get(&start, TIME_UTC);
        verify_tokens(bi, tokens, tokens_len, &true_positive, &predicted_positive); // measure metric
        timespec_get(&end, TIME_UTC);

        // metrics calculation
        delta_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
        double precision = (((double) true_positive) / (double) predicted_positive);
        double recall = (((double) true_positive) / (double) expected_positive);
        double f1 = 2 * precision * recall / (precision + recall);

        if (f1 < worst_f1) {
            worst_f1 = f1;
        }

        if (f1 > best_f1) {
            best_f1 = f1;
        }

        if (sim > 0) {
            double prev_mean = mean_f1;
            double prev_tokenize_time = mean_tokenization;
            mean_f1 = ((mean_f1 * sim) + f1) / (sim + 1); 
            mean_tokenization = ((mean_tokenization * sim) + delta_ms) / (sim + 1);
            var_f1 = var_f1 + (f1 - prev_mean) * (f1 - mean_f1);
            var_tokenization = var_tokenization + (delta_ms - prev_tokenize_time) * (delta_ms - mean_tokenization);
        } else {
            mean_f1 = f1;
            mean_tokenization = delta_ms;
        }

        printf("F1-score = %f\n", f1);
        printf("Simulation %d total time to verify token %llu ms\n", sim, delta_ms);
    }
    
    ubrk_close(bi);
    free(shuffled);

    printf("Average F1 score = %f\n", mean_f1);
    printf("F1 variance = %f\n", var_f1 / ((double) MONTE_CARLO - 1));
    printf("Best F1 score = %f\n", best_f1);
    printf("Worst F1 score = %f\n", worst_f1);
    printf("Margin of error at 95%% for F1 = %f\n", 1.984 * sqrt(var_f1 / ((double) MONTE_CARLO - 1)) / sqrt((double) MONTE_CARLO - 1));
    printf("Margin of error at 99%% for F1 = %f\n", 2.626 * (var_f1 / sqrt((double) MONTE_CARLO - 1)));
    printf("Mean tokenizer tokenization time = %f ms\n", mean_tokenization);
    printf("Var tokenizer tokenization time = %f ms\n", var_tokenization/ ((double) MONTE_CARLO - 1));
    printf("Margin of error at 95%% for tokenization time = %f\n", 1.984 * (var_tokenization / sqrt((double) MONTE_CARLO - 1)) / sqrt(((double) MONTE_CARLO - 1)));
}

int main(void) {
    char* path = "data/lexitron_mod.txt";
    int sz = get_size(path);
    int32_t* tokens = malloc(sz * sizeof(int32_t) * 2); // 2d array of token slice
    size_t result_len = 0;
    size_t tokens_len = 0;
    printf("Dictinoary file size in byte = %d", sz);
    UFILE* f = u_fopen(path, "r", "th_TH", "utf8");
    // char* bytes = malloc(sz);
    UChar* chars = malloc(sz * sizeof(UChar));
    printf("Start loading dictionary\n");
    load_dict(f, chars, tokens, &result_len, &tokens_len);
    printf("Dictionary successfully loaded\n");
    printf("Total tokens in dictionary = %zu\n", tokens_len / 2);
    printf("Total concatenated characters in text = %zu\n", result_len);
    
    // shuffle tokens position
    monte_carlo_sim(chars, result_len, tokens, tokens_len);

    free(tokens);
    free(chars);
    // free(bytes);
    u_fclose(f);
}