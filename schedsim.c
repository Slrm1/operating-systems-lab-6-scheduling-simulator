#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "process.h"
#include "util.h"

/* Round Robin: cycle through unfinished processes, granting one quantum each. */
/* The required RR baseline uses the original ready-queue order; arrivals are time zero. */
void findWaitingTimeRR(ProcessType plist[], int n, int quantum)
{
    int *rem_bt;

    if (n <= 0 || quantum <= 0) {
        return;
    }
    rem_bt = calloc((size_t)n, sizeof(*rem_bt));
    int time = 0;
    int done;

    if (rem_bt == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; ++i) {
        rem_bt[i] = plist[i].bt;
        plist[i].wt = 0;
    }

    do {
        done = 1;
        /* One pass represents one complete rotation of the ready queue. */
        for (int i = 0; i < n; ++i) {
            if (rem_bt[i] > 0) {
                done = 0;
                if (rem_bt[i] > quantum) {
                    time += quantum;
                    rem_bt[i] -= quantum;
                } else {
                    time += rem_bt[i];
                    plist[i].wt = time - plist[i].bt;
                    rem_bt[i] = 0;
                }
            }
        }
    } while (!done);

    free(rem_bt);
}

/* Preemptive SJF (SRTF): execute the arrived job with least work left. */
void findWaitingTimeSJF(ProcessType plist[], int n)
{
    int *remaining;

    if (n <= 0) {
        return;
    }
    remaining = calloc((size_t)n, sizeof(*remaining));
    int completed = 0;
    int current_time = 0;

    if (remaining == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; ++i) {
        remaining[i] = plist[i].bt;
        plist[i].wt = 0;
    }

    while (completed < n) {
        int shortest = -1;
        int minimum = INT_MAX;

        /* Strict less-than preserves FCFS order when remaining times tie. */
        for (int i = 0; i < n; ++i) {
            if (plist[i].art <= current_time && remaining[i] > 0 &&
                remaining[i] < minimum) {
                minimum = remaining[i];
                shortest = i;
            }
        }

        if (shortest == -1) {
            ++current_time;
            continue;
        }

        --remaining[shortest];
        ++current_time;

        if (remaining[shortest] == 0) {
            int waiting;
            ++completed;
            waiting = current_time - plist[shortest].art - plist[shortest].bt;
            plist[shortest].wt = waiting < 0 ? 0 : waiting;
        }
    }

    free(remaining);
}

/* The required baseline treats every arrival as time zero, matching the supplied reference. */
void findWaitingTime(ProcessType plist[], int n)
{
    if (n <= 0) {
        return;
    }
    plist[0].wt = plist[0].art;
    for (int i = 1; i < n; ++i) {
        plist[i].wt = plist[i - 1].bt + plist[i - 1].wt;
    }
}

void findTurnAroundTime(ProcessType plist[], int n)
{
    for (int i = 0; i < n; ++i) {
        plist[i].tat = plist[i].bt + plist[i].wt;
    }
}

/* Higher numeric priority runs first; equal priorities retain FCFS/PID order. */
int my_comparer(const void *this, const void *that)
{
    const ProcessType *left = (const ProcessType *)this;
    const ProcessType *right = (const ProcessType *)that;

    if (left->pri != right->pri) {
        return right->pri - left->pri;
    }
    return left->pid - right->pid;
}

void findavgTimeFCFS(ProcessType plist[], int n)
{
    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nFCFS\n");
}

void findavgTimeSJF(ProcessType plist[], int n)
{
    findWaitingTimeSJF(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nSJF\n");
}

void findavgTimeRR(ProcessType plist[], int n, int quantum)
{
    findWaitingTimeRR(plist, n, quantum);
    findTurnAroundTime(plist, n);
    printf("\n*********\nRR Quantum = %d\n", quantum);
}

void findavgTimePriority(ProcessType plist[], int n)
{
    qsort(plist, (size_t)n, sizeof(*plist), my_comparer);
    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nPriority\n");
}

void printMetrics(ProcessType plist[], int n)
{
    int total_wt;

    if (n <= 0) {
        printf("No processes to schedule\n");
        return;
    }
    total_wt = 0;
    int total_tat = 0;
    float awt;
    float att;

    printf("\tProcesses\tBurst time\tWaiting time\tTurn around time\n");
    for (int i = 0; i < n; ++i) {
        total_wt += plist[i].wt;
        total_tat += plist[i].tat;
        printf("\t%d\t\t%d\t\t%d\t\t%d\n", plist[i].pid,
               plist[i].bt, plist[i].wt, plist[i].tat);
    }

    awt = (float)total_wt / (float)n;
    att = (float)total_tat / (float)n;
    printf("\nAverage waiting time = %.2f", awt);
    printf("\nAverage turn around time = %.2f\n", att);
}

ProcessType *initProc(char *filename, int *n)
{
    FILE *input_file = fopen(filename, "r");
    ProcessType *plist;

    if (input_file == NULL) {
        fprintf(stderr, "Error: Invalid file path\n");
        exit(EXIT_FAILURE);
    }
    plist = parse_file(input_file, n);
    fclose(input_file);
    if (plist == NULL || *n <= 0) {
        free(plist);
        fprintf(stderr, "Error: input contains no processes\n");
        exit(EXIT_FAILURE);
    }
    return plist;
}

static ProcessType *cloneProcesses(const ProcessType *source, int n)
{
    ProcessType *copy;

    if (source == NULL || n <= 0) {
        return NULL;
    }
    copy = malloc((size_t)n * sizeof(*copy));
    if (copy == NULL) {
        fprintf(stderr, "Unable to allocate process copy\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, source, (size_t)n * sizeof(*copy));
    return copy;
}

int main(int argc, char *argv[])
{
    int n = 0;
    const int quantum = 2;
    ProcessType *original;
    ProcessType *proc_list;

    if (argc < 2) {
        fprintf(stderr, "Usage: ./schedsim <input-file-path>\n");
        return EXIT_FAILURE;
    }

    /* Read once, then isolate each simulation with an independent deep copy. */
    original = initProc(argv[1], &n);

    proc_list = cloneProcesses(original, n);
    findavgTimeFCFS(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    proc_list = cloneProcesses(original, n);
    findavgTimeSJF(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    proc_list = cloneProcesses(original, n);
    findavgTimePriority(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    proc_list = cloneProcesses(original, n);
    findavgTimeRR(proc_list, n, quantum);
    printMetrics(proc_list, n);
    free(proc_list);

    free(original);
    return EXIT_SUCCESS;
}
