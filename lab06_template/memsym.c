#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#define TRUE 1
#define FALSE 0
#define NUM_PROCESSES 4
#define TLB_SIZE 8

// Output file
FILE* output_file;

// TLB replacement strategy (FIFO or LRU)
char* strategy;

// Global timestamp counter
uint32_t timestamp_counter = 0;

// Memory parameters
int OFF_BITS = -1;
int PFN_BITS = -1;
int VPN_BITS = -1;
int defined = FALSE;

// Physical memory
uint32_t* physical_memory = NULL;
int memory_size = 0;

// Page Table Entry
typedef struct {
    uint32_t pfn;
    int valid;
} PageTableEntry;

// TLB Entry
typedef struct {
    uint32_t vpn;
    uint32_t pfn;
    int valid;
    int pid;
    uint32_t timestamp;
} TLBEntry;

// Process state
typedef struct {
    uint32_t r1;
    uint32_t r2;
    PageTableEntry* page_table;
} ProcessState;

// Global state
ProcessState processes[NUM_PROCESSES];
TLBEntry tlb[TLB_SIZE];
int current_pid = 0;

void initialize_simulator(int off, int pfn, int vpn) {
    OFF_BITS = off;
    PFN_BITS = pfn;
    VPN_BITS = vpn;
    
    // Allocate physical memory
    memory_size = 1 << (OFF_BITS + PFN_BITS);
    physical_memory = (uint32_t*)calloc(memory_size, sizeof(uint32_t));
    
    // Initialize page tables for all processes
    int num_pages = 1 << VPN_BITS;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        processes[i].r1 = 0;
        processes[i].r2 = 0;
        processes[i].page_table = (PageTableEntry*)calloc(num_pages, sizeof(PageTableEntry));
    }
    
    // Initialize TLB
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = FALSE;
    }
    
    defined = TRUE;
    fprintf(output_file, "Current PID: %d. Memory instantiation complete. OFF bits: %d. PFN bits: %d. VPN bits: %d\n", 
            current_pid, OFF_BITS, PFN_BITS, VPN_BITS);
}

void handle_define(char** tokens) {
    if (defined) {
        fprintf(output_file, "Current PID: %d. Error: multiple calls to define in the same trace\n", current_pid);
        fclose(output_file);
        exit(0);
    }
    
    int off = atoi(tokens[1]);
    int pfn = atoi(tokens[2]);
    int vpn = atoi(tokens[3]);
    
    initialize_simulator(off, pfn, vpn);
}

void handle_ctxswitch(char** tokens) {
    int new_pid = atoi(tokens[1]);
    
    if (new_pid < 0 || new_pid >= NUM_PROCESSES) {
        fprintf(output_file, "Current PID: %d. Invalid context switch to process %d\n", current_pid, new_pid);
        fclose(output_file);
        exit(0);
    }
    
    current_pid = new_pid;
    fprintf(output_file, "Current PID: %d. Switched execution context to process: %d\n", current_pid, current_pid);
}

void handle_map(char** tokens) {
    uint32_t vpn = atoi(tokens[1]);
    uint32_t pfn = atoi(tokens[2]);
    
    // Update page table
    processes[current_pid].page_table[vpn].pfn = pfn;
    processes[current_pid].page_table[vpn].valid = TRUE;
    
    // Update or add to TLB
    int tlb_index = -1;
    
    // First, check if this VPN is already in TLB for current process
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].pid == current_pid && tlb[i].vpn == vpn) {
            tlb_index = i;
            break;
        }
    }
    
    // If not found, find an invalid entry or use replacement strategy
    if (tlb_index == -1) {
        for (int i = 0; i < TLB_SIZE; i++) {
            if (!tlb[i].valid) {
                tlb_index = i;
                break;
            }
        }
    }
    
    // If all valid, use replacement strategy
    if (tlb_index == -1) {
        uint32_t oldest_timestamp = tlb[0].timestamp;
        tlb_index = 0;
        for (int i = 1; i < TLB_SIZE; i++) {
            if (tlb[i].timestamp < oldest_timestamp) {
                oldest_timestamp = tlb[i].timestamp;
                tlb_index = i;
            }
        }
    }
    
    // Update TLB entry
    tlb[tlb_index].vpn = vpn;
    tlb[tlb_index].pfn = pfn;
    tlb[tlb_index].valid = TRUE;
    tlb[tlb_index].pid = current_pid;
    tlb[tlb_index].timestamp = timestamp_counter;
    
    fprintf(output_file, "Current PID: %d. Mapped virtual page number %u to physical frame number %u\n", 
            current_pid, vpn, pfn);
}

void handle_unmap(char** tokens) {
    uint32_t vpn = atoi(tokens[1]);
    
    // Invalidate page table entry
    processes[current_pid].page_table[vpn].valid = FALSE;
    
    // Invalidate TLB entry if present
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].pid == current_pid && tlb[i].vpn == vpn) {
            tlb[i].valid = FALSE;
            break;
        }
    }
    
    fprintf(output_file, "Current PID: %d. Unmapped virtual page number %u\n", current_pid, vpn);
}

void handle_pinspect(char** tokens) {
    uint32_t vpn = atoi(tokens[1]);
    PageTableEntry* pte = &processes[current_pid].page_table[vpn];
    
    fprintf(output_file, "Current PID: %d. Inspected page table entry %u. Physical frame number: %u. Valid: %d\n",
            current_pid, vpn, pte->pfn, pte->valid);
}

void handle_tinspect(char** tokens) {
    int tlbn = atoi(tokens[1]);
    TLBEntry* entry = &tlb[tlbn];
    
    fprintf(output_file, "Current PID: %d. Inspected TLB entry %d. VPN: %u. PFN: %u. Valid: %d. PID: %d. Timestamp: %u\n",
            current_pid, tlbn, entry->vpn, entry->pfn, entry->valid, entry->pid, entry->timestamp);
}

void handle_linspect(char** tokens) {
    uint32_t pl = atoi(tokens[1]);
    uint32_t value = physical_memory[pl];
    
    fprintf(output_file, "Current PID: %d. Inspected physical location %u. Content: %u\n",
            current_pid, pl, value);
}

void handle_rinspect(char** tokens) {
    char* reg = tokens[1];
    uint32_t value;
    
    if (strcmp(reg, "r1") == 0) {
        value = processes[current_pid].r1;
    } else if (strcmp(reg, "r2") == 0) {
        value = processes[current_pid].r2;
    } else {
        fprintf(output_file, "Current PID: %d. Error: invalid register operand %s\n", current_pid, reg);
        fclose(output_file);
        exit(0);
    }
    
    fprintf(output_file, "Current PID: %d. Inspected register %s. Content: %u\n",
            current_pid, reg, value);
}

char** tokenize_input(char* input) {
    char** tokens = NULL;
    char* token = strtok(input, " ");
    int num_tokens = 0;

    while (token != NULL) {
        num_tokens++;
        tokens = realloc(tokens, num_tokens * sizeof(char*));
        tokens[num_tokens - 1] = malloc(strlen(token) + 1);
        strcpy(tokens[num_tokens - 1], token);
        token = strtok(NULL, " ");
    }

    num_tokens++;
    tokens = realloc(tokens, num_tokens * sizeof(char*));
    tokens[num_tokens - 1] = NULL;

    return tokens;
}

int main(int argc, char* argv[]) {
    const char usage[] = "Usage: memsym.out <strategy> <input trace> <output trace>\n";
    char* input_trace;
    char* output_trace;
    char buffer[1024];

    if (argc != 4) {
        printf("%s", usage);
        return 1;
    }
    strategy = argv[1];
    input_trace = argv[2];
    output_trace = argv[3];

    FILE* input_file = fopen(input_trace, "r");
    output_file = fopen(output_trace, "w");  

    while (!feof(input_file)) {
        char *rez = fgets(buffer, sizeof(buffer), input_file);
        if (!rez) {
            break;
        }
        
        // Remove endline character
        if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }
        
        // Skip empty lines
        if (strlen(buffer) == 0) {
            continue;
        }
        
        // Skip comments
        if (buffer[0] == '%') {
            continue;
        }
        
        char** tokens = tokenize_input(buffer);
        
        // Check if tokens is valid
        if (tokens == NULL || tokens[0] == NULL) {
            for (int i = 0; tokens && tokens[i] != NULL; i++)
                free(tokens[i]);
            free(tokens);
            continue;
        }
        
        // Check if define was called
        if (strcmp(tokens[0], "define") != 0 && !defined) {
            fprintf(output_file, "Current PID: %d. Error: attempt to execute instruction before define\n", current_pid);
            for (int i = 0; tokens[i] != NULL; i++)
                free(tokens[i]);
            free(tokens);
            fclose(input_file);
            fclose(output_file);
            exit(0);
        }
        
        // Increment timestamp for each instruction
        timestamp_counter++;
        
        // Handle commands
        if (strcmp(tokens[0], "define") == 0) {
            handle_define(tokens);
        } else if (strcmp(tokens[0], "ctxswitch") == 0) {
            handle_ctxswitch(tokens);
        } else if (strcmp(tokens[0], "map") == 0) {
            handle_map(tokens);
        } else if (strcmp(tokens[0], "unmap") == 0) {
            handle_unmap(tokens);
        } else if (strcmp(tokens[0], "pinspect") == 0) {
            handle_pinspect(tokens);
        } else if (strcmp(tokens[0], "tinspect") == 0) {
            handle_tinspect(tokens);
        } else if (strcmp(tokens[0], "linspect") == 0) {
            handle_linspect(tokens);
        } else if (strcmp(tokens[0], "rinspect") == 0) {
            handle_rinspect(tokens);
        }
        // TODO: Add load, store, add instructions
        
        // Deallocate tokens
        for (int i = 0; tokens[i] != NULL; i++)
            free(tokens[i]);
        free(tokens);
    }

    fclose(input_file);
    fclose(output_file);
    
    // Cleanup
    if (physical_memory) free(physical_memory);
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (processes[i].page_table) free(processes[i].page_table);
    }

    return 0;
}