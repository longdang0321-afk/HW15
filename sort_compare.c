#include "sort_compare.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct Node {
    int key, count, height;
    struct Node *left, *right;
} Node;

typedef struct {
    SortStats *stats;
    size_t heap_bytes;
} Context;

/* 재귀 호출의 인자·지역 변수 저장공간을 동일한 기준으로 추정한다.
   실제 호출 프레임의 크기는 컴파일러에 따라 달라진다. */
#define FRAME_BYTES (3 * sizeof(void *) + 4 * sizeof(int))

static void *allocate(size_t bytes)
{
    void *p = malloc(bytes);
    if (!p) { fprintf(stderr, "Memory allocation failed.\n"); exit(EXIT_FAILURE); }
    return p;
}
static void memory(Context *c, int depth, size_t extra)
{
    size_t bytes = sizeof(*c) + sizeof(SortStats) + c->heap_bytes
                 + (size_t)depth * FRAME_BYTES + extra;
    if (bytes > c->stats->peak_bytes) c->stats->peak_bytes = bytes;
}
static int equal(int a, int b, Context *c)
{
    c->stats->comparisons++;
    return a == b;
}
static int less(int a, int b, Context *c)
{
    c->stats->comparisons++;
    return a < b;
}
static void swap(int *a, int *b, Context *c)
{
    if (a == b) return;
    int temp = *a; *a = *b; *b = temp;
    c->stats->movements += 3;
}
/* 첫 원소 피벗 + Lomuto 분할 */
static void quick_sort(int *a, int left, int right, Context *c, int depth)
{
    memory(c, depth, 0);
    if (left >= right) return;
    swap(&a[left], &a[right], c);
    int pivot = a[right], pos = left;
    c->stats->movements++;
    for (int j = left; j < right; j++) {
        c->stats->comparisons++;
        if (a[j] <= pivot) swap(&a[pos++], &a[j], c);
    }
    swap(&a[pos], &a[right], c);
    quick_sort(a, left, pos - 1, c, depth + 1);
    quick_sort(a, pos + 1, right, c, depth + 1);
}
static int height(Node *p) { return p ? p->height : 0; }
static void update(Node *p)
{
    int l = height(p->left), r = height(p->right);
    p->height = 1 + (l > r ? l : r);
}
static int balance(Node *p) { return height(p->left) - height(p->right); }
static Node *rotate_right(Node *p)
{
    Node *q = p->left;
    p->left = q->right; q->right = p;
    update(p); update(q);
    return q;
}
static Node *rotate_left(Node *p)
{
    Node *q = p->right;
    p->right = q->left; q->left = p;
    update(p); update(q);
    return q;
}
static Node *rebalance(Node *p)
{
    update(p);
    if (balance(p) > 1) {
        if (balance(p->left) < 0) p->left = rotate_left(p->left);
        return rotate_right(p);
    }
    if (balance(p) < -1) {
        if (balance(p->right) > 0) p->right = rotate_right(p->right);
        return rotate_left(p);
    }
    return p;
}
static Node *insert(Node *p, int value, Context *c, int depth)
{
    memory(c, depth, 0);
    if (!p) {
        p = allocate(sizeof(*p));
        p->key = value; p->count = p->height = 1;
        p->left = p->right = NULL;
        c->stats->movements++;
        c->heap_bytes += sizeof(*p);
        memory(c, depth, 0);
        return p;
    }
    if (equal(value, p->key, c)) { p->count++; return p; }
    if (less(value, p->key, c)) p->left = insert(p->left, value, c, depth + 1);
    else p->right = insert(p->right, value, c, depth + 1);
    return rebalance(p);
}
/* 후계자의 count까지 옮겼으므로 최솟값 노드 전체를 제거한다. */
static Node *remove_min(Node *p, Context *c, int depth)
{
    memory(c, depth, 0);
    if (!p->left) {
        Node *child = p->right;
        free(p); c->heap_bytes -= sizeof(Node);
        return child;
    }
    p->left = remove_min(p->left, c, depth + 1);
    return rebalance(p);
}
static Node *delete(Node *p, int value, Context *c, int depth)
{
    memory(c, depth, 0);
    if (!p) return NULL;
    if (equal(value, p->key, c)) {
        c->stats->deleted++;
        if (p->count > 1) { p->count--; return p; }
        if (!p->left || !p->right) {
            Node *child = p->left ? p->left : p->right;
            free(p); c->heap_bytes -= sizeof(Node);
            return child;
        }
        Node *next = p->right;
        while (next->left) next = next->left;
        p->key = next->key; p->count = next->count;
        c->stats->movements++;
        p->right = remove_min(p->right, c, depth + 1);
    } else if (less(value, p->key, c)) p->left = delete(p->left, value, c, depth + 1);
    else p->right = delete(p->right, value, c, depth + 1);
    return rebalance(p);
}
static void inorder(Node *p, int *out, int *pos, Context *c, int depth, size_t extra)
{
    memory(c, depth, extra);
    if (!p) return;
    inorder(p->left, out, pos, c, depth + 1, extra);
    for (int i = 0; i < p->count; i++) {
        out[(*pos)++] = p->key;
        c->stats->movements++;
    }
    inorder(p->right, out, pos, c, depth + 1, extra);
}
/* 검증과 최종 해제는 측정 구간 밖에서 수행한다. */
static int validate(Node *p, long long low, long long high)
{
    if (!p) return 0;
    assert(low < p->key && p->key < high && p->count > 0);
    int l = validate(p->left, low, p->key), r = validate(p->right, p->key, high);
    assert(abs(l - r) <= 1 && p->height == 1 + (l > r ? l : r));
    return p->height;
}
static void destroy(Node *p)
{
    if (!p) return;
    destroy(p->left); destroy(p->right); free(p);
}
void run_quick(const int *source, int n, const Operation *ops, int m,
               int *result, SortStats *s)
{
    *s = (SortStats){0};
    Context c = {s, (size_t)n * sizeof(int)};
    int capacity = n, count = n;
    int *a = allocate(c.heap_bytes);
    for (int i = 0; i < n; i++) a[i] = source[i];
    clock_t start = clock();
    quick_sort(a, 0, n - 1, &c, 1);
    s->sort_comparisons = s->comparisons; s->sort_movements = s->movements;
    clock_t sorted_end = clock();
    for (int i = 1; i < n; i++) assert(a[i - 1] <= a[i]);
    clock_t operations_start = clock();
    for (int k = 0; k < m; k++) {
        int pos = 0, value = ops[k].value;
        while (pos < count && less(a[pos], value, &c)) pos++;
        if (ops[k].insert) {
            if (count == capacity) {
                /* 배열이 찼을 때만 확장한다. 남은 최대 삽입 수 m만큼 확보한다.
                   복사 이동과 확장 중 두 배열이 차지하는 공간도 포함한다. */
                int *b = allocate((size_t)(capacity + m) * sizeof(int));
                memory(&c, 0, (size_t)(capacity + m) * sizeof(int));
                for (int i = 0; i < count; i++) { b[i] = a[i]; s->movements++; }
                free(a); a = b; capacity += m;
                c.heap_bytes = (size_t)capacity * sizeof(int);
            }
            for (int i = count; i > pos; i--) { a[i] = a[i - 1]; s->movements++; }
            a[pos] = value; s->movements++; count++; s->inserted++;
        } else if (pos < count && equal(a[pos], value, &c)) {
            for (int i = pos; i < count - 1; i++) { a[i] = a[i + 1]; s->movements++; }
            count--; s->deleted++;
        }
    }
    clock_t end = clock();
    s->total_ms = 1000.0 * ((sorted_end - start) + (end - operations_start)) / CLOCKS_PER_SEC;
    s->final_count = count;
    for (int i = 0; i < count; i++) result[i] = a[i];
    free(a);
}
void run_avl(const int *source, int n, const Operation *ops, int m,
             int *result, SortStats *s)
{
    *s = (SortStats){0};
    Context c = {s, 0};
    Node *root = NULL;
    clock_t start = clock();
    for (int i = 0; i < n; i++) root = insert(root, source[i], &c, 1);
    int *sorted = allocate((size_t)n * sizeof(int)), pos = 0;
    inorder(root, sorted, &pos, &c, 1, (size_t)n * sizeof(int));
    s->sort_comparisons = s->comparisons; s->sort_movements = s->movements;
    clock_t sorted_end = clock();
    assert(pos == n);
    for (int i = 1; i < n; i++) assert(sorted[i - 1] <= sorted[i]);
    free(sorted);
    clock_t operations_start = clock();
    for (int i = 0; i < m; i++) {
        if (ops[i].insert) { root = insert(root, ops[i].value, &c, 1); s->inserted++; }
        else root = delete(root, ops[i].value, &c, 1);
    }
    clock_t end = clock();
    s->total_ms = 1000.0 * ((sorted_end - start) + (end - operations_start)) / CLOCKS_PER_SEC;
    s->final_count = n + s->inserted - s->deleted;
    validate(root, LLONG_MIN, LLONG_MAX);
    SortStats saved = *s;  /* 최종 검증 순회의 계측값은 결과에서 제외한다. */
    pos = 0; inorder(root, result, &pos, &c, 1, 0);
    assert(pos == s->final_count);
    *s = saved;
    destroy(root);
}
