#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

// total jobs
int numofjobs = 0;

struct job
{
    // job id is ordered by the arrival; jobs arrived first have smaller job id, always increment by 1
    int id;
    int arrival; // arrival time; safely assume the time unit has the minimal increment of 1
    int length;
    int tickets; // number of tickets for lottery scheduling
    // metadata for analysis / scheduling
    int start_time;  // first time the job starts running (Ts)
    int finish_time; // completion time (Tc)
    int remaining;   // remaining execution time (for preemptive policies)
    int wait_time;   // accumulated wait time
    struct job *next;
};

// the workload list
struct job *head = NULL;

void append_to(struct job **head_pointer, int arrival, int length, int tickets)
{

    struct job *new_job = malloc(sizeof(*new_job));
    if (!new_job)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    new_job->id = numofjobs++;
    new_job->arrival = arrival;
    new_job->length = length;
    new_job->tickets = tickets;
    new_job->start_time = -1;
    new_job->finish_time = -1;
    new_job->remaining = length;
    new_job->wait_time = 0;
    new_job->next = NULL;

    if (*head_pointer == NULL)
    {
        *head_pointer = new_job;
        return;
    }

    struct job *cur = *head_pointer;
    while (cur->next)
        cur = cur->next;
    cur->next = new_job;

    new_job->remaining_time = length;

    return;
}

void read_job_config(const char *filename)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int tickets = 0;

    char *delim = ",";
    char *arrival = NULL;
    char *length = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0)
    {
        fprintf(stderr, "Error: Input file '%s' is empty.\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    rewind(fp);

    while ((read = getline(&line, &len, fp)) != -1)
    {
        if (line[read - 1] == '\n')
            line[read - 1] = 0;
        arrival = strtok(line, delim);
        length = strtok(NULL, delim);
        tickets += 100;

        append_to(&head, atoi(arrival), atoi(length), tickets);
    }

    fclose(fp);
    if (line)
        free(line);
}

void policy_SJF()
{
    printf("Execution trace with SJF:\n");

    // TODO: implement SJF policy

    printf("End of execution with SJF.\n");
}

void policy_STCF()
{
    printf("Execution trace with STCF:\n");
    
    int current_time = 0;
    int num_completed = 0;

    // convert linked list to array for easy access
    struct job *jobs[numofjobs];
    struct job *curr = head;
    int i = 0;
    while (curr) {
        jobs[i++] = curr;
        curr = curr->next;
    }

    struct job *current_job = NULL;

    while (num_completed < numofjobs) {
        // find the job with the shortest remaining time among arrived jobs
        int shortest_idx = -1;
        int shortest_remaining = INT_MAX;

        for (int j = 0; j < numofjobs; j++) {
            if (jobs[j]->remaining_time > 0 && jobs[j]->arrival <= current_time) {
                if (jobs[j]->remaining_time < shortest_remaining) {
                    shortest_remaining = jobs[j]->remaining_time;
                    shortest_idx = j;
                }
            }
        }

        if (shortest_idx == -1) {
            // no jobs have arrived yet
            current_time++;
            continue;
        }

        struct job *job = jobs[shortest_idx];

        // if this is the first time running this job
        if (job->remaining_time == job->length)
            printf("t=%d: Job %d starts (arrival=%d, length=%d)\n", current_time, job->id, job->arrival, job->length);
        else if (current_job != job)
            printf("t=%d: Switch to Job %d\n", current_time, job->id);

        current_job = job;

        // run for 1 time unit
        job->remaining_time--;
        current_time++;

        if (job->remaining_time == 0) {
            job->finish_time = current_time;
            num_completed++;
            printf("t=%d: Job %d finished\n", current_time, job->id);
        }
    }


    printf("End of execution with STCF.\n");
}

void policy_RR(int slice)
{
    printf("Execution trace with RR:\n");

    printf("End of execution with RR.\n");
}

void policy_LT(int slice)
{
    printf("Execution trace with LT:\n");

    srand(42); // deterministic
    int current_time = 0;
    int finished_jobs = 0;

    while (finished_jobs < numofjobs)
    {
        int total_tickets = 0;
        struct job *cur;

        // sum tickets of ready jobs
        for (cur = head; cur != NULL; cur = cur->next)
        {
            if (cur->arrival <= current_time && cur->remaining > 0)
                total_tickets += cur->tickets;
        }

        // no ready job, advance time
        if (total_tickets == 0)
        {
            current_time++;
            continue;
        }

        // choose winner
        int winning_ticket = rand() % total_tickets;
        int cumulative = 0;
        struct job *selected = NULL;

        for (cur = head; cur != NULL; cur = cur->next)
        {
            if (cur->arrival <= current_time && cur->remaining > 0)
            {
                cumulative += cur->tickets;
                if (cumulative > winning_ticket)
                {
                    selected = cur;
                    break;
                }
            }
        }

        // first run timestamp
        if (selected->start_time == -1)
            selected->start_time = current_time;

        int run_time = min(slice, selected->remaining);

        printf("t=%d: [Job %d] arrived at [%d], ran for: [%d]\n",
               current_time, selected->id, selected->arrival, run_time);

        selected->remaining -= run_time;
        current_time += run_time;

        if (selected->remaining == 0)
        {
            selected->finish_time = current_time;
            selected->wait_time = selected->finish_time - selected->arrival - selected->length;
            finished_jobs++;
        }
    }

    printf("End of execution with LT.\n");
}

void policy_FIFO()
{
    printf("Execution trace with FIFO:\n");

    int current_time = 0;
    struct job *cur = head;

    while (cur)
    {
        // if job hasn't arrived yet, advance time to its arrival
        if (current_time < cur->arrival)
            current_time = cur->arrival;

        // job starts now
        cur->start_time = current_time;
        printf("t=%d: [Job %d] arrived at [%d], ran for: [%d]\n",
               current_time, cur->id, cur->arrival, cur->length);

        // run to completion
        current_time += cur->length;

        // job finishes
        cur->finish_time = current_time;
        cur->wait_time = cur->start_time - cur->arrival;

        cur = cur->next;
    }

    printf("End of execution with FIFO.\n");
}

int main(int argc, char **argv)
{

    static char usage[] = "usage: %s analysis policy slice trace\n";

    int analysis;
    char *pname;
    char *tname;
    int slice;

    if (argc < 5)
    {
        fprintf(stderr, "missing variables\n");
        fprintf(stderr, usage, argv[0]);
        exit(1);
    }

    // if 0, we don't analysis the performance
    analysis = atoi(argv[1]);

    // policy name
    pname = argv[2];

    // time slice, only valid for RR
    slice = atoi(argv[3]);

    // workload trace
    tname = argv[4];

    read_job_config(tname);

    if (strcmp(pname, "FIFO") == 0)
    {
        policy_FIFO();
        if (analysis == 1)
        {
            printf("Begin analyzing FIFO:\n");
            struct job *cur2 = head;
            int count = 0;
            double total_response = 0;
            double total_turnaround = 0;
            double total_wait = 0;

            while (cur2)
            {
                int response = cur2->start_time - cur2->arrival;
                int turnaround = cur2->finish_time - cur2->arrival;
                int wait = cur2->wait_time;

                printf("Job %d -- Response time: %d Turnaround: %d Wait: %d\n",
                       cur2->id, response, turnaround, wait);

                total_response += response;
                total_turnaround += turnaround;
                total_wait += wait;
                count++;
                cur2 = cur2->next;
            }

            if (count > 0)
            {
                printf("Average -- Response: %.2f Turnaround %.2f Wait %.2f\n",
                       total_response / count,
                       total_turnaround / count,
                       total_wait / count);
            }
            printf("End analyzing FIFO.\n");
        }
    }
    else if (strcmp(pname, "SJF") == 0)
    {
        // TODO
    }
    else if (strcmp(pname, "STCF") == 0)
    {
        // TODO
    }
    else if (strcmp(pname, "RR") == 0)
    {
    }
    else if (strcmp(pname, "LT") == 0)
    {
        policy_LT(slice);
        if (analysis == 1)
        {
            printf("Begin analyzing LT:\n");
            struct job *cur2 = head;
            int count = 0;
            double total_response = 0;
            double total_turnaround = 0;
            double total_wait = 0;

            while (cur2)
            {
                int response = cur2->start_time - cur2->arrival;
                int turnaround = cur2->finish_time - cur2->arrival;
                int wait = cur2->wait_time;

                printf("Job %d -- Response time: %d  Turnaround: %d  Wait: %d\n",
                       cur2->id, response, turnaround, wait);

                total_response += response;
                total_turnaround += turnaround;
                total_wait += wait;
                count++;
                cur2 = cur2->next;
            }

            if (count > 0)
            {
                printf("Average -- Response: %.2f  Turnaround %.2f  Wait %.2f\n",
                       total_response / count,
                       total_turnaround / count,
                       total_wait / count);
            }
            printf("End analyzing LT.\n");
        }
    }

    exit(0);
}