#ifndef SORT_COMPARE_H
#define SORT_COMPARE_H

#include <stddef.h>

typedef struct {
    int insert;                 /* 1: 삽입, 0: 삭제 */
    int value;
} Operation;

typedef struct {
    unsigned long long comparisons;
    unsigned long long movements;
    unsigned long long sort_comparisons;
    unsigned long long sort_movements;
    size_t peak_bytes;
    double total_ms;
    int final_count;
    int inserted;
    int deleted;
} SortStats;

/* result에는 모든 연산 후의 오름차순 데이터를 저장한다. */
void run_quick(const int *source, int n, const Operation *ops, int m,
               int *result, SortStats *stats);
void run_avl(const int *source, int n, const Operation *ops, int m,
             int *result, SortStats *stats);

#endif
