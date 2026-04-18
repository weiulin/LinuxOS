#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_GET_THREAD_INFO 450

// 定義thread_info結構
struct my_thread_info_record{
    unsigned long pid;
    unsigned long tgid;
    void *process_descriptor_address;
    void *kernel_mode_stack_address;
    void *pgd_table_address;
}data;

struct data_{
    int id;
    char name[16];
};
typedef struct data_ sdata;
static __thread sdata tx; //thread local variable

int my_get_thread_kernel_info(struct my_thread_info_record *data){
    return syscall(SYS_GET_THREAD_INFO, data);
}

void hello(int pid){
    if (my_get_thread_kernel_info(&data)){
        printf("pid=%u\n", data.pid);
        printf("tgid=%u\n", data.tgid);
        printf("process descriptor address =%p\n", data.process_descriptor_address);
        printf("kernel mode stack address =%p\n", data.kernel_mode_stack_address);
        printf("pgd table address =%p\n", data.pgd_table_address);
    }
    else{
        printf("Cannot execute the new system call correctly\n");
    }
}

void *func1(void *arg){
    char *p = (char*) arg;
    int pid;

    pid = syscall(__NR_gettid); // 取得執行緒 ID
    tx.id = pid;
    strcpy(tx.name, p);

    printf("I am thread with ID %d executing func1().\n", pid);
    hello(pid);
    while(1){
        //printf("(%d)(%s)\n",tx.id,tx.name);
        sleep(1);
    }
}

void *func2(void *arg) {
    char *p = (char*) arg;
    int pid;

    pid = syscall(__NR_gettid);
    tx.id = pid;
    strcpy(tx.name, p);

    printf("I am thread with ID %d executing func2().\n", pid);
    hello(pid);

    while(1){
         //printf("(%d)(%s)\n",tx.id,tx.name);
        sleep(2);
    }
}

int main() {
    pthread_t id[2];
    char p[2][16];
    strcpy(p[0], "Thread1");
    pthread_create(&id[0], NULL, func1, (void *)p[0]);
    // if (pthread_create(&id[0], NULL, func1, (void *)p[0]) != 0) {
    //     perror("pthread_create 1 failed");
    //     return 1;
    // }

    strcpy(p[1], "Thread2");
    pthread_create(&id[1], NULL, func2, (void *)p[1]);
    // if (pthread_create(&id[1], NULL, func2, (void *)p[1]) != 0) {
    //     perror("pthread_create 2 failed");
    //     return 1;
    // }

    int pid;
    pid = syscall(__NR_gettid);
    tx.id = pid;
    strcpy(tx.name, "MAIN");

    printf("I am main thread with ID %d.\n", pid);
    hello(pid);
    while(1) {
        //printf("(%d)(%s)\n",tx.id,tx.name);
        sleep(5);
    }
}
