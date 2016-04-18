#include <stdio.h>
#include <string.h>

#include "stm32f3xx_hal.h"
#include "stm32f3_discovery.h"
#include "stm32f3_discovery_accelerometer.h"
#include "stm32f3_discovery_gyroscope.h"

#include "common.h"

/*  CPU manual is here: */
/*  http://www.st.com/web/en/resource/technical/document/programming_manual/DM00046982.pdf */

#define MAX_TASKS 5
#define STACK_WORDS 1000


/*  This is the structure that represents a single process. */
struct task_descriptor{
    unsigned int * sp;
    unsigned int priority;
    struct task_descriptor* next;
};

struct scheduler{
    struct task_descriptor * main_descriptor;
    struct task_descriptor * currently_running_task;
    struct task_descriptor * p0_queue;
    struct task_descriptor * p1_queue;
    struct task_descriptor * p2_queue;
};

extern void release_processor( void );
extern unsigned int get_ticks( void );
extern void switch_stacks( void );

int prompt_Setup(void);
void schedule_next_task(struct scheduler *);
void enqueue_ready_task(struct scheduler *, struct task_descriptor *);
void queue_initialize(struct task_descriptor **, unsigned int);
struct task_descriptor * queue_pop_start(struct task_descriptor **);
int queue_push_end(struct task_descriptor **, struct task_descriptor *);
void init_scheduler(struct scheduler *);
void c_SVC_Handler(unsigned int * sp, unsigned int);
static void SystemClock_Config(void);
unsigned int * init_task_stack(void (*)(void), unsigned int *);

void *_malloc (size_t size);
void _free (void* ptr);

unsigned int stack5[STACK_WORDS];
unsigned int stack4[STACK_WORDS];
unsigned int stack3[STACK_WORDS];
unsigned int stack2[STACK_WORDS];
unsigned int stack1[STACK_WORDS];

unsigned int * next_sp;
struct scheduler scheduler;
struct task_descriptor* all_task_descriptors [MAX_TASKS + 1];

void init_scheduler(struct scheduler * scheduler){
    memset(scheduler, 0, sizeof (struct scheduler));
    queue_initialize (&(scheduler->p0_queue), MAX_TASKS + 1);
    queue_initialize (&(scheduler->p1_queue), MAX_TASKS + 1);
    queue_initialize (&(scheduler->p2_queue), MAX_TASKS + 1);
}

void enqueue_ready_task(struct scheduler * scheduler, struct task_descriptor * task){
    struct task_descriptor ** queue = NULL;

    switch (task->priority)
    {
        case 0:
            queue = &(scheduler->p0_queue);
            break;
        case 1:
            queue = &(scheduler->p1_queue);
            break;
        case 2:
            queue = &(scheduler->p2_queue);
            break;
        default:
            printf ("error");
            return;
            break;
    }

    if (queue != NULL)
    {
        queue_push_end (queue, task);
    }

    return;
}

void schedule_next_task(struct scheduler * scheduler) {
    struct task_descriptor * popped;
    if (scheduler->p0_queue != NULL) {
        popped = queue_pop_start(&scheduler->p0_queue);
    }
    else if (scheduler->p1_queue != NULL) {
        popped = queue_pop_start(&scheduler->p1_queue);
    }
    else if (scheduler->p2_queue != NULL) {
        popped = queue_pop_start(&scheduler->p2_queue);
    }
    else {
        printf ("error");
        return;
    }
    scheduler->currently_running_task = popped;
}

void queue_initialize(struct task_descriptor ** queue, unsigned int size) {
    *queue = NULL;
    return;
}

int queue_push_end(struct task_descriptor ** queue, struct task_descriptor * item) {
    if (*queue == NULL) {
        (*queue) = item->next = item;
        return 0;
    }
    else {
        item->next = (*queue)->next;
        (*queue)->next = item;
        (*queue) = item;
        return 1;
    }
}

struct task_descriptor * queue_pop_start(struct task_descriptor ** queue) {
    struct task_descriptor * task = (*queue);

    if (task == NULL) {
        return task;
    }
    else if (task == task->next) {
        (*queue) = NULL;
    }
    else {
        task = task->next;
        (*queue)->next = task->next;
    }

    return task;
}

void do_kernel_call(unsigned int *);

void do_kernel_call(unsigned int * sp){
    unsigned int R0_offset = 1 + 8; /*  Offsets that tell us how far down the stack the information we need is */
    unsigned int R1_offset = 2 + 8;
    unsigned int PC_offset = 7 + 8;
    /*  Take the copied SP value, and account for the recently added lr, then consider the 6 exception frame 
        values so that pc_instruction_ptr points to the stored value of PC in the exception frame. */
    unsigned short * pc_instruction_ptr = (unsigned short *)*(sp + PC_offset);
    /*  The svc instruction that resulted in this SVC_Handler call is located
        one 16 bit instruction before the saved value of the program counter. */
    unsigned short svc_instruction = *(pc_instruction_ptr -1);
    /*  The SVC function call number is encoded in the instruction in the lower 8 bits.  */
    unsigned int call_number = svc_instruction & 0xFF;
    (void)R0_offset;
    (void)R1_offset;

    switch(call_number){
        case 1:{
                   /*  Release processor, do nothing */
                   break;
               }default:{
                   printf ("error");
                   return;
               }
    }
}

void c_SVC_Handler(unsigned int * sp, unsigned int entry_type){
    /*
       At this point, a task's stack looks like this:

       +------------------------------------------------------------------------+
       |   32 bit item on stack                                | How to Access  |
       +-------Manually Pushed General Purpose Registers------------------------+
       |   Register r11                                        |     *(sp + 0)  |  <-  Top of Stack
       +------------------------------------------------------------------------+
       |   Register r10                                        |     *(sp + 1)  |
       +------------------------------------------------------------------------+
       |   Register ...  (r9, r8, r7, r6, r5)                  |     *(sp + n)  |
       +------------------------------------------------------------------------+
       |   Register r4                                         |     *(sp + 7)  |
       +------------------------------------------------------------------------+
       |   LR (for return from exepction) = FFFFFFF9           |     *(sp + 8)  |  
       +-----------------------Exception Stack Frame----------------------------+
       |   R0 = First function param, also return value after  |     *(sp + 9)  |
       +------------------------------------------------------------------------+
       |   R1 = Second function parameter                      |     *(sp +10)  |
       +------------------------------------------------------------------------+
       |   R2 = Third input param                              |     *(sp +11)  |
       +------------------------------------------------------------------------+
       |   R3 = Fourth input param                             |     *(sp +12)  |
       +------------------------------------------------------------------------+
       |   R12                                                 |     *(sp +13)  |
       +------------------------------------------------------------------------+
       |   LR (for function call that got to svc instruction)  |     *(sp +14)  |
       +------------------------------------------------------------------------+
       |   PC = address of next instruction after svc call     |     *(sp +15)  |
       +------------------------------------------------------------------------+
       |   xPSR = saved status flags                           |     *(sp +16)  |  <-  Bottom of Stack
       +------------------------------------------------------------------------+
       */

    /*  Save current task's SP */
    scheduler.currently_running_task->sp = sp; /* Points directly at the special FFFFFF** value */
    if(scheduler.currently_running_task != scheduler.main_descriptor){
        enqueue_ready_task(&scheduler, scheduler.currently_running_task); /*  Don't enqueue the 'main' method, so we don't schedule it */
    }

    /*  We can enter into the kernel using different methods, and may wish to handle this differently */
    switch(entry_type){
        case 0:{
                   do_kernel_call(sp);
                   break;
               }case 1:{
                   /*  We entered the kernel from an interrupt */
                   /*  You could include code here to decode what kind of interrupt fired and handle each one differently. */
                   break;
               }default:{
                   printf ("error");
                   return;
               }
    }

    schedule_next_task(&scheduler);
    next_sp = scheduler.currently_running_task->sp;
    switch_stacks();
}

unsigned int * init_task_stack(void (*task)(void), unsigned int * sp){
    /*  Set up an exception stack frame for a new process */
    *sp = 0x01000000;   /* xPSR */
    sp--;
    *sp = (unsigned int)task; /* Initial PC value */
    sp--;
    *sp = 0; /* LR value (Should never return from a user proc, this would be an error) */
    sp--;
    *sp = 0; /* Initial R12 value */
    sp--;
    *sp = 0; /* Initial R3 value */
    sp--;
    *sp = 0; /* Initial R2 value */
    sp--;
    *sp = 0; /* Initial R1 value */
    sp--;
    *sp = 0; /* Initial R0 value */
    sp--;
    *sp = 0xFFFFFFF9; /* Value for RTE */
    sp -= 8;  /* 8 registers for general purpose registers r4-r11 */
    return sp;
}

void task_4(void){
    unsigned int i = 0;
    while(1){
        if(i % 50000 == 0){
            printf("Roger Bongers\n");
        }
        i++;
        release_processor();
    }
}

void task_3(void){
    unsigned int i = 0;
    while(1){
        if(i % 50000 == 0){
            printf("T3\n");
        }
        i++;
        release_processor();
    }
}

void task_2(void){
    unsigned int i = 0;
    while(1){
        if(i % 50000 == 0){
            printf("T2\n");
        }
        i++;
        release_processor();
    }
}

void task_1(void){
    unsigned int i = 0;
    while(1){
        if(i % 50000 == 0){
            printf("T1\n");
            if (i > 100000) {
                // free self from queue: will never see this posted again
                _free (all_task_descriptors[1]);
                all_task_descriptors[1] = NULL;
                scheduler.currently_running_task = all_task_descriptors[0];
            }
        }
        i++;
        release_processor();
    }
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow : 
 *            System Clock source            = PLL (HSE)
 *            SYSCLK(Hz)                     = 72000000
 *            HCLK(Hz)                       = 72000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 2
 *            APB2 Prescaler                 = 1
 *            HSE Frequency(Hz)              = 8000000
 *            HSE PREDIV                     = 1
 *            PLLMUL                         = RCC_PLL_MUL9 (9)
 *            Flash Latency(WS)              = 2
 * @param  None
 * @retval None
 */
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
    {
        Error_Handler();
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
       clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2)!= HAL_OK)
    {
        Error_Handler();
    }
}

#define HEAP_SIZE ((MAX_TASKS + 1) * sizeof (struct task_descriptor))
#define abs(x) ((x < 0) ? -x : x)

unsigned char* get_heap(void) {
    static unsigned char heap [HEAP_SIZE];
    return heap;
}

int* get_free_list(void) {
    static int free_list [HEAP_SIZE] = { HEAP_SIZE, 0 };
    return free_list;
}

void *_malloc (size_t size) {
    unsigned char* heap = get_heap ();
    int* free_list = get_free_list ();
    int nsize = size;
    int i;

    if (nsize <= 0) {
        return NULL;
    }

    // find first available block
    for (i = 0; i < HEAP_SIZE && nsize > free_list [i]; i += abs (free_list [i])) {
        // fatal error
        if (free_list [i] == 0) {
            return NULL;
        }
    };
    if (i >= HEAP_SIZE || free_list [i] <= 0) {
        return NULL;
    }

    // split block
    if (free_list [i] != nsize) {
        free_list [i + nsize] = free_list [i] - nsize;
    }

    // mark as allocated
    free_list [i] = -nsize;
    return &(heap [i]);
}

void _free (void* ptr) {
    unsigned char* heap = get_heap ();
    int* free_list = get_free_list ();
    int index = (int) ptr - (int) heap;
    int i, tmp;

    if (index < 0 || index >= HEAP_SIZE || free_list [index] >= 0) {
        return;
    }

    // accumulate successive free blocks
    i = index - free_list [index];
    while (i < HEAP_SIZE && free_list [i] > 0) {
        tmp = free_list [i];
        free_list [i] = 0;
        i += tmp;
    }
    // mark block as free
    free_list [index] = i - index;
}

int main(int argc, char **argv) {
    unsigned int k = 0;

    getchar();

    /* Configure the system clock */
    SystemClock_Config();
    /*  Sets up a bunch of stuff so you can connect with minicom, and use a prompt */
    prompt_Setup();

    init_scheduler(&scheduler);

    /*  We'll use task descriptor 0 to represent our entry from here in main */
    all_task_descriptors[0] = _malloc (sizeof (struct task_descriptor));
    all_task_descriptors[0]->sp = 0;
    all_task_descriptors[0]->priority = 0;
    scheduler.main_descriptor = all_task_descriptors[0];

    /*  Our user tasks */
    all_task_descriptors[1] = _malloc (sizeof (struct task_descriptor));
    all_task_descriptors[1]->sp = init_task_stack(task_1, &stack1[STACK_WORDS -1]);
    all_task_descriptors[1]->priority = 0;
    all_task_descriptors[2] = _malloc (sizeof (struct task_descriptor));
    all_task_descriptors[2]->sp = init_task_stack(task_2, &stack2[STACK_WORDS -1]);
    all_task_descriptors[2]->priority = 0;
    all_task_descriptors[3] = _malloc (sizeof (struct task_descriptor));
    all_task_descriptors[3]->sp = init_task_stack(task_3, &stack3[STACK_WORDS -1]);
    all_task_descriptors[3]->priority = 0;
    all_task_descriptors[4] = _malloc (sizeof (struct task_descriptor));
    all_task_descriptors[4]->sp = init_task_stack(task_4, &stack4[STACK_WORDS -1]);
    all_task_descriptors[4]->priority = 0;

    enqueue_ready_task(&scheduler, all_task_descriptors[1]);
    enqueue_ready_task(&scheduler, all_task_descriptors[2]);
    enqueue_ready_task(&scheduler, all_task_descriptors[3]);
    enqueue_ready_task(&scheduler, all_task_descriptors[4]);

    scheduler.currently_running_task = all_task_descriptors[0];

    while(1){
        if(k % 400000 == 0){
            printf("Main thread %d\n", k);
        }
        k++;
        TaskInput();
        release_processor();
    }
    return 0;
}
