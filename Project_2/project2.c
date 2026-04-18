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
