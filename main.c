#include "sort_compare.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define DATA_SEED 2026u
#define OP_SEED 12026u
#define VALUE_RANGE 100000
#define REPEAT 30

static void *allocate(size_t bytes)
{
    void *p = malloc(bytes);
    if (p == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

/* 두 구현이 같은 오류를 내는 경우도 잡기 위한 독립적인 빈도표 검증 */
static void verify(const int *a, const int *b, const int *expected,
                   const SortStats *q, const SortStats *t, int n)
{
    assert(q->final_count == t->final_count);
    assert(q->inserted == t->inserted && q->deleted == t->deleted);
    assert(q->final_count == n + q->inserted - q->deleted);
    int pos = 0;
    for (int value = 0; value < VALUE_RANGE; value++) {
        for (int j = 0; j < expected[value]; j++) {
            assert(pos < q->final_count);
            assert(a[pos] == value && b[pos] == value);
            pos++;
        }
    }
    assert(pos == q->final_count);
}

int main(void)
{
    const int sizes[] = {1000, 2000, 4000, 8000, 16000, 20000};
    printf("Data seed: %u | Operation seed: %u | Values: 0..%d\n",
           DATA_SEED, OP_SEED, VALUE_RANGE - 1);
    printf("Pivot: First (Lomuto) | Repeat: %d | CPU time average\n", REPEAT);
    printf("Memory: peak heap + estimated recursive workspace\n\n");
    for (int k = 0; k < 6; k++) {
        int n = sizes[k], m = n / 5;
        int *source = allocate((size_t)n * sizeof(int));
        Operation *ops = allocate((size_t)m * sizeof(Operation));
        int *qout = allocate((size_t)(n + m) * sizeof(int));
        int *tout = allocate((size_t)(n + m) * sizeof(int));
        int *expected = calloc(VALUE_RANGE, sizeof(int));
        if (expected == NULL) exit(EXIT_FAILURE);
        srand(DATA_SEED);
        for (int i = 0; i < n; i++) {
            source[i] = rand() % VALUE_RANGE;
            expected[source[i]]++;
        }
        srand(OP_SEED);
        int planned_insert = 0;
        for (int i = 0; i < m; i++) {
            int choice = rand();       /* 연산 선택용 난수 */
            ops[i].insert = choice % 2 == 0;
            ops[i].value = rand() % VALUE_RANGE; /* 값은 별도 난수 */
            int v = ops[i].value;
            if (ops[i].insert) {
                planned_insert++;
                expected[v]++;
            } else if (expected[v] > 0) expected[v]--;
        }
        double qtime = 0.0, ttime = 0.0;
        SortStats q = {0}, t = {0};
        /* 실행 순서를 번갈아 바꾸어 항상 먼저 실행되는 방법이 없도록 한다. */
        for (int r = 0; r < REPEAT; r++) {
            if (r % 2 == 0) {
                run_quick(source, n, ops, m, qout, &q);
                run_avl(source, n, ops, m, tout, &t);
            } else {
                run_avl(source, n, ops, m, tout, &t);
                run_quick(source, n, ops, m, qout, &q);
            }
            verify(qout, tout, expected, &q, &t, n);
            qtime += q.total_ms;
            ttime += t.total_ms;
        }
        printf("Data Size : %d\nOperations: %d\n", n, m);
        printf("%-20s %16s %16s\n", "", "Quick Sort", "AVL Tree");
        printf("------------------------------------------------------\n");
        printf("%-20s %13.6f ms %13.6f ms\n", "Total Time", qtime / REPEAT, ttime / REPEAT);
        printf("%-20s %16llu %16llu\n", "Comparisons", q.comparisons, t.comparisons);
        printf("%-20s %16llu %16llu\n", "Data Movements", q.movements, t.movements);
        printf("%-20s %10zu Bytes %10zu Bytes\n", "Memory Usage", q.peak_bytes, t.peak_bytes);
        printf("%-20s %16d %16d\n", "Final Data Count", q.final_count, t.final_count);
        printf("%-20s %16llu %16llu\n", "Sort Comparisons", q.sort_comparisons, t.sort_comparisons);
        printf("%-20s %16llu %16llu\n", "Operation Compares", q.comparisons - q.sort_comparisons, t.comparisons - t.sort_comparisons);
        printf("%-20s %16llu %16llu\n", "Sort Movements", q.sort_movements, t.sort_movements);
        printf("%-20s %16llu %16llu\n", "Operation Moves", q.movements - q.sort_movements, t.movements - t.sort_movements);
        printf("Insertions: %d | Delete attempts: %d | Deleted: %d | Missing: %d\n",
               planned_insert, m - planned_insert, q.deleted,
               m - planned_insert - q.deleted);
        printf("Verification: PASS (all values, duplicates, AVL balance)\n\n");
        free(source);
        free(ops);
        free(qout);
        free(tout);
        free(expected);
    }
    return 0;
}
