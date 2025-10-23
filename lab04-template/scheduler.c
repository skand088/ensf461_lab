#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define min(a,b) (((a)<(b))?(a):(b))

// total jobs
int numofjobs = 0;

struct job {
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
    int done;        // 0 = not finished, 1 = finished
    struct job *next;

};

// the workload list
struct job *head = NULL;


void append_to(struct job **head_pointer, int arrival, int length, int tickets){

    struct job *new_job = malloc(sizeof(*new_job));
    if (!new_job) {
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
    new_job->done = 0;
    new_job->next = NULL;

    if (*head_pointer == NULL) {
        *head_pointer = new_job;
        return;
    }

    struct job *cur = *head_pointer;
    while (cur->next)
        cur = cur->next;
    cur->next = new_job;

    return;
}


void read_job_config(const char* filename)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int tickets  = 0;

    char* delim = ",";
    char *arrival = NULL;
    char *length = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file '%s'\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
        if (ftell(fp) == 0) {
            fprintf(stderr, "Error: Input file '%s' is empty.\n", filename);
            fclose(fp);
            exit(EXIT_FAILURE);
            }
            rewind(fp);

    while ((read = getline(&line, &len, fp)) != -1)
    {
        if( line[read-1] == '\n' )
            line[read-1] =0;
        arrival = strtok(line, delim);
        length = strtok(NULL, delim);
        tickets += 100;

        append_to(&head, atoi(arrival), atoi(length), tickets);
    }

    fclose(fp);
    if (line) free(line);
}


void policy_SJF()
{
    printf("Execution trace with SJF:\n");

    int current_time = 0;
    int remaining = numofjobs;
    struct job *chosen;
    struct job *p;

    
    while (remaining > 0) {
        chosen = NULL;
        for (p = head; p != NULL; p = p->next) {
            if (p->done) continue;
            if (p->arrival <= current_time) {
                if (chosen == NULL
                    || p->length < chosen->length
                    || (p->length == chosen->length && p->arrival < chosen->arrival)
                    || (p->length == chosen->length && p->arrival == chosen->arrival && p->id < chosen->id))
                {
                    chosen = p;
                }
            }
        }

        if (chosen == NULL) {
            int next_arrival = INT_MAX;
            for (p = head; p != NULL; p = p->next) {
                if (p->done) continue;
                if (p->arrival < next_arrival) next_arrival = p->arrival;
            }
            current_time = next_arrival;
            continue; 
        }

        // start chosen job
        int start = current_time < chosen->arrival ? chosen->arrival : current_time;
     chosen->start_time = start;
     printf("t=%d: [Job %d] arrived at [%d], ran for: [%d]\n",
         start, chosen->id, chosen->arrival, chosen->length);

        // run to completion
        current_time = start + chosen->length;
        chosen->finish_time = current_time;
        // for non-preemptive SJF, wait time = start - arrival
        chosen->wait_time = chosen->start_time - chosen->arrival;
        chosen->done = 1;
        remaining--;

    /* finished message omitted to match FIFO-style output (start-time lines only) */
    }

    printf("End of execution with SJF.\n");

}


void policy_STCF()
{
    printf("Execution trace with STCF:\n");

    // TODO: implement STCF policy

    printf("End of execution with STCF.\n");
}


void policy_RR(int slice)
{
    printf("Execution trace with RR:\n");

    // TODO: implement RR policy

    printf("End of execution with RR.\n");
}


void policy_LT(int slice)
{
    printf("Execution trace with LT:\n");

    // Leave this here, it will ensure the scheduling behavior remains deterministic
    srand(42);

    // In the following, you'll need to:
    // Figure out which active job to run first
    // Pick the job with the shortest remaining time
    // Considers jobs in order of arrival, so implicitly breaks ties by choosing the job with the lowest ID

    // To achieve consistency with the tests, you are encouraged to choose the winning ticket as follows:
    // int winning_ticket = rand() % total_tickets;
    // And pick the winning job using the linked list approach discussed in class, or equivalent

    printf("End of execution with LT.\n");

}


void policy_FIFO(){
    printf("Execution trace with FIFO:\n");

    int current_time = 0;
    struct job *cur = head;

    while (cur) {
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
    cur->done = 1;


        cur = cur->next;
    }

    printf("End of execution with FIFO.\n");
}


int main(int argc, char **argv){

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

    if (strcmp(pname, "FIFO") == 0){
        policy_FIFO();
        if (analysis == 1){
            printf("Begin analyzing FIFO:\n");
            struct job *cur2 = head;
            int count = 0;
            double total_response = 0;
            double total_turnaround = 0;
            double total_wait = 0;

            while (cur2) {
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

            if (count > 0) {
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
        policy_SJF();
        if (analysis == 1) {
            printf("Begin analyzing SJF:\n");
            struct job *cur2 = head;
            int count = 0;
            double total_response = 0;
            double total_turnaround = 0;
            double total_wait = 0;

            while (cur2) {
                int response = cur2->start_time - cur2->arrival;
                int turnaround = cur2->finish_time - cur2->arrival;
                int wait = cur2->start_time - cur2->arrival;

                printf("Job %d -- Response time: %d Turnaround: %d Wait: %d\n",
                       cur2->id, response, turnaround, wait);

                total_response += response;
                total_turnaround += turnaround;
                total_wait += wait;
                count++;
                cur2 = cur2->next;
            }

            if (count > 0) {
                printf("Average -- Response: %.2f Turnaround %.2f Wait %.2f\n",
                       total_response / count,
                       total_turnaround / count,
                       total_wait / count);
            }
            printf("End analyzing SJF.\n");
        }
    }
    else if (strcmp(pname, "STCF") == 0)
    {
        // TODO
    }
    else if (strcmp(pname, "RR") == 0)
    {
        // TODO
    }
    else if (strcmp(pname, "LT") == 0)
    {
        // TODO
    }

	exit(0);
}