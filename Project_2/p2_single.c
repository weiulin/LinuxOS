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
