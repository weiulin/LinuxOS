# Linux 作業系統 project 2

## 第15組
### 組員
* 114522001 林憶茹
* 114522091 劉彥霆
* 114522141 林威佑
* Z1140096 王柏霖
### 系統環境
* 虛擬機：VMware Workstation Pro
* 作業系統：Ubuntu 24.04
* Kernel版本：5.15.137
* Memory：4GB
* Processors： 4
* Disk： 30GB

## Question
<details>
    <summary>Description</summary>
    
- In this project, you need to define new data type
```c
struct my_thread_info_record{
    unsigned long pid;
    unsigned long tgid;
    void *process_descriptor_address;
    void *kernel_mode_stack_address;
    void *pgd_table_address;
};
```
and write a new system call 
```c
int my_get_thread_kernel_info(void *)
```
so that a thread can use the new system call to collect some information of a thread.
- The return value of this system call is an integer. 0 means that an error occurs when executing this system call. A non-zero value means that the system is executed successfully. You can also utilize the return value to tranfer information you need.

</details>

### ==Kernel Space Code (Implement Code)==
```c
#include <linux/kernel.h>
#include <linux/syscalls.h>   // 提供 SYSCALL_DEFINE1
#include <linux/sched.h>      // 提供 task_struct
#include <linux/uaccess.h>    // 提供 copy_to_user
#include <linux/mm_types.h>   // 提供 mm_struct (存取 pgd)

// 定義thread_info結構
struct my_thread_info_record{
    unsigned long pid;
    unsigned long tgid;
    void *process_descriptor_address;
    void *kernel_mode_stack_address;
    void *pgd_table_address;
};


SYSCALL_DEFINE1(my_get_thread_kernel_info, struct my_thread_info_record __user *, user_record){
    struct my_thread_info_record record;
    
    // 取得當前thread的 task_struct
    struct task_struct *task = current;

    // 將對應資料從task_struct取出
    // task pid
    record.pid = task->pid;
    
    // task tgid
    record.tgid = task->tgid;
    
    // process_descriptor_address
    record.process_descriptor_address = (void *)task;
    
    // kernel_mode_stack_address 通常存在 task->stack 中
    record.kernel_mode_stack_address = (void *)task->stack;
    
    // pgd_table_address (Page Global Directory)
    // 只有當 process 擁有記憶體空間時 (mm != NULL) 才有 PGD
    if (task->mm){
        record.pgd_table_address = (void *)task->mm->pgd;
    } else{
        record.pgd_table_address = NULL; // Kernel thread 可能沒有 mm
    }

    if (copy_to_user(user_record, &record, sizeof(struct my_thread_info_record))){
        return 0; 
    }

    return 1; 
}
```
### ==User Space Code (Test Code)==
```c
#include <stdio.h>
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

int my_get_thread_kernel_info(void *data){
    return syscall(SYS_GET_THREAD_INFO, data);
}

int main(){
    if(my_get_thread_kernel_info(&data)){
        printf("pid=%u\n", data.pid);
        printf("tgid=%u\n", data.tgid);
        printf("process descriptor address =%p\n", data.process_descriptor_address);
        printf("kernel mode stack address =%p\n",data.kernel_mode_stack_address);
        printf("pgd table address =%p\n", data.pgd_table_address);
    } else {
        printf("Cannot execute the new system call correctly\n");
    }

    return 0;
}
```
### Result
![p2_single](https://hackmd.io/_uploads/B1N4Tirf-g.png)

pid=2990 且 tgid=2990，在 Linux 中，pid 表示 user 視角的 thread id，tgid 表示 user 視角的 process id。這驗證了它是一個單一執行緒 Process。


### ==User Space Code (Test Code)==
```c
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_GET_THREAD_INFO 450

extern void *func1(void *);
extern void *func2(void *);
extern int main();

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

    pid = syscall(__NR_gettid); 
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
```
### Result
![p2_multi](https://hackmd.io/_uploads/BknSpsBfbl.png)

Main Thread: pid=3004, tgid=3004

Func1 Thread: pid=3005, tgid=3004

Func2 Thread: pid=3006, tgid=3004

三個執行緒擁有 不同的 PID (代表它們在 Kernel 中是獨立的 task_struct)，但擁有 相同的 TGID (3004)，表示它們屬於同個 Process Group (也就是使用者視角中的同一個Process)。

三個 threads 的 process descriptor address 都不同，在 Linux kernel 中，不論 thread 還是 process 都是一個 task_struct。

三個 threads 的 page table address 都同樣是 0xffff936c57d1a000 表示同一個 process 裡的 multithead 共用一個 page table。

三個 threads 的 kernel mode stack address 都不同，因為 stack 表示了該 threads 執行狀態, 進度等等，每個 threads 不應該完全相同。