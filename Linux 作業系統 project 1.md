# Linux 作業系統 project 1

## 第15組
### 組員
* 114522001 林憶茹
* 114522091 劉彥霆
* 114522141 林威佑
* 我是神經很大條的外校生我忘記學號了去demo再補 王柏霖
### 系統環境
* 虛擬機：VMware Workstation Pro
* 作業系統：Ubuntu 24.04
* Kernel版本：5.15.137
* Memory：4GB
* Processors： 4
* Disk： 30GB
## Translate Virtual Address to Physical Address 
<details>
    <summary>Description</summary>
    
- In modern operating systems, every running program is a process, and each process has its own virtual address space. The CPU and OS cooperate through the page table to translate a virtual address (VA) to its corresponding physical address (PA) in memory.
- In this project, you will implement a new Linux system call that translates a given virtual address into its corresponding physical address. This allows you to directly observe how the OS manages physical memory and how virtual-to-physical mappings evolve at runtime.
- Your system call should follow the prototype below:
```c
void * my_get_physical_addresses(void *)
```
- The return value should be:
    - 0 -> if the virtual address is currently not mapped to any physical page
    - Non-zero value -> the physical address corresponding to the input virtual address
</details>

### ==Kernel Space Code (Implement Code)==

```c
#include <linux/kernel.h>   // 提供 `unsigned long` 型別和 `PAGE_SHIFT` / `PAGE_MASK` 巨集
#include <linux/syscalls.h> // 提供 `SYSCALL_DEFINE1` 巨集，用來「定義」system call
#include <linux/mm.h>       // 提供高階記憶體結構 (如 `mm_struct`, `vm_area_struct`) 和檢查函式 (如 `find_vma`, `access_ok`)
#include <linux/sched.h>    // 提供 `current` 變數，用來取得目前正在執行的 process
#include <linux/pgtable.h>  // 提供走訪Page Table所需的所有函式和型別 (如 `pgd_offset`, `pte_t`, `pte_pfn`)

/*
 * 這個系統呼叫會接收一個使用者虛擬位址 (vaddr)
 * 回傳對應的實體位址 (paddr)
 * 若該虛擬位址尚未分配實體頁框，則回傳 0。
 */

SYSCALL_DEFINE1(my_get_physical_addresses, void __user *, vaddr) {

    unsigned long virt_addr = (unsigned long)vaddr;
    struct mm_struct *mm = current->mm; // 取得process的memory descriptor
    struct vm_area_struct *vma;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    phys_addr_t phys = 0; // default為no mapped

    /* 排除沒有VA space, e.g. kernel thread */
    if (!mm)
        return 0;
    
    /* checks that the pointer is in the user space range */
    if (!access_ok(vaddr, 1))
        return 0;
    
    /* checks that the pointer is valid */
    vma = find_vma(mm, virt_addr);
    if (!vma || virt_addr < vma->vm_start)
        return 0;
    
    /* 逐層走訪頁表：PGD → P4D → PUD → PMD → PTE */
    mmap_read_lock(mm);

    pgd = pgd_offset(mm, virt_addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        goto out;
    
    p4d = p4d_offset(pgd, virt_addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        goto out;
    
    pud = pud_offset(p4d, virt_addr);
    if (pud_none(*pud) || pud_bad(*pud))
        goto out;
    
    /* 1GB huge page */
    if (pud_leaf(*pud)) {
        unsigned long pfn = pud_pfn(*pud);
        unsigned long offset = virt_addr & ~PUD_PAGE_MASK;

        phys = ((phys_addr_t)pfn << PUD_SHIFT) | offset;
        goto out;
    }
    
    /* 2MB huge page */
    pmd = pmd_offset(pud, virt_addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        goto out;

    if (pmd_leaf(*pmd)) {
        unsigned long pfn = pmd_pfn(*pmd);
        unsigned long offset = virt_addr & ~PMD_PAGE_MASK;

        phys = ((phys_addr_t)pfn << PMD_SHIFT) | offset;
        goto out;
    }
    
    /* 
     * 這邊我不確定，要check一下
     * 是需求建立 physical frame 到 HighMEM 的 temporary mapping
     * 在 64-bit 系統下不需要此種 mapping。
     */

    /*
     *因為32位元系統的虛擬空間不足以direct-map到整個physical address 
     *所以才要temporary mapping 
     * 64位元的不用，因為可以mapping的數量夠大
    */
    
    pte = pte_offset_map(pmd, virt_addr);
    if (!pte)
        goto out;
    
    /* 如果該頁尚未分配實體記憶體 */
    if (!pte_present(*pte)) {
        pte_unmap(pte);
        goto out;
    }

    phys = ((phys_addr_t)pte_pfn(*pte) << PAGE_SHIFT) | (virt_addr & ~PAGE_MASK);

    pte_unmap(pte);

    out:
        mmap_read_unlock(mm);
        return (unsigned long)phys;
}
```
<details>
    <summary>Old version</summary>
    
```c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <asm/page.h>

/*
 * 這個系統呼叫會接收一個使用者虛擬位址 (vaddr)
 * 回傳對應的實體位址 (physical address)
 * 若該虛擬位址尚未分配實體頁框，則回傳 0。
 */
SYSCALL_DEFINE1(my_get_physical_addresses, void __user *, vaddr)
{
    struct mm_struct *mm = current->mm;   // 取得目前行程的記憶體描述
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long virt_addr = (unsigned long)vaddr;
    unsigned long page_addr = 0;
    unsigned long page_offset = 0;
    unsigned long paddr = 0;

    /* 檢查目前行程的 mm_struct 是否有效 */
    if (!mm)
        return 0;

    /* 逐層走訪頁表：PGD → P4D → PUD → PMD → PTE */
    pgd = pgd_offset(mm, virt_addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return 0;

    p4d = p4d_offset(pgd, virt_addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return 0;

    pud = pud_offset(p4d, virt_addr);
    if (pud_none(*pud) || pud_bad(*pud))
        return 0;

    pmd = pmd_offset(pud, virt_addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return 0;
    
    pte = pte_offset_map(pmd, virt_addr);
    if (!pte)
        return 0;

    /* 如果該頁尚未分配實體記憶體 */
    if (!pte_present(*pte)) {
        pte_unmap(pte);
        return 0;
    }

    /* 計算實體位址：PFN << PAGE_SHIFT + 頁內偏移 */
    page_addr = (pte_pfn(*pte) << PAGE_SHIFT);
    page_offset = virt_addr & ~PAGE_MASK;
    paddr = page_addr | page_offset;

    pte_unmap(pte);

    // printk("[my_get_physical_addresses] VA=0x%lx -> PA=0x%lx\n", virt_addr, paddr);

    return (long)paddr;
}
```
    
</details>
    
### ==Code 說明==
![image](https://hackmd.io/_uploads/rkjKV9rl-x.png)
1. current->mm 存有 PGD 的 Base Address 以及 list of memory areas
    ![image](https://hackmd.io/_uploads/By7zS9SeZx.png)
2. 在確認使用者給予 valid 的 vma 後逐步走訪 PGD、P4D、PUD、PMD、PTE，其中根據啟用的page table level，P4D, PUD, PMD 會依序不被使用，相應的 pxd_offset() 會直接回傳輸入值。
3. 在如圖 x86-64 的 4-level 架構中，虛擬地址內指向 PGD、PPUD、PMD、PTE 的 page table 各有 9 bits (2^3 * 2^9 = 4KB)，最後的 offset 有 12 bits。
4. 走訪的核心迴圈邏輯是：
    a.用「上一層 Entry」取得「本層頁表的基底位址」。（current->mm->pgd 是第一層的基底位址） 
    b.用「VA 中對應的索引」在本層頁表中「定位到 Entry」。
5. 最後會查找到 PTE，其內容包含映射到的 PFN，以及 Control Bits。
6. 將 VA 的最後 12 bits offset 替換掉 Control Bits，即可得到對應的 physical address。
7. Linux 也有支援 1GB, 2MB 的 huge page，以類似 4,5,6 的流程得到 physical address，只是變成 pud_pfn | 30-bits offset 以及 pmd_pfn | 21-bits offset。



## Question 1
<details>
    <summary>Description</summary>
    
- In this part, we use `malloc()` to allocate memory. At first, the virtual addresses (VAs) are reserved, but they do not yet have corresponding physical addresses (PAs).
- Only when the program writes to a page does the operating system actually allocate a physical page.
- This mechanism, where physical memory is allocated only on first access, is called **lazy allocation**.

</details>

### ==User Space Code (Test Code)==
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define SYS_GET_PHY  449   // <-- 這裡換成自訂義 syscall number


unsigned long my_get_physical_addresses(void *va) {
   return syscall(SYS_GET_PHY, va);
}


int main(void) {
   unsigned long length = 4 * 4096;


   // malloc
   void *addr = malloc(length);
   if (addr == NULL) {
       perror("mmap");
       return 0;
   }


   printf("malloc() returned address = %p\n\n", addr);
   printf("Check VA -> PA before access:\n");


   for (size_t i = 0; i < length; i += 4096) {
       unsigned long pa = my_get_physical_addresses((void *)addr + i);
       printf("  VA: %p -> PA: %p\n", (char *)addr + i, (void *)pa);
   }


   printf("\nTouching memory (write 1 byte per page)...\n");
   for (size_t i = 0; i < length; i += 4096) {
       ((char *)addr)[i] = 42;
   }


   printf("\nCheck VA -> PA after access:\n");
   for (size_t i = 0; i < length; i += 4096) {
       unsigned long pa = my_get_physical_addresses((void *)addr + i);
       printf("  VA: %p -> PA: %p\n", (char *)addr + i, (void *)pa);
   }


   return 0;
}
```
### Result
![image](https://hackmd.io/_uploads/HJGvDT3JWl.png)
1. lazy allocation : 擴展 vma areas 但不實際分配 physical frame，直到該 page 確實被使用時才分配並建立 mapping。是為了避免分配了卻不使用的空間浪費 (e.g. arr[31415926535897932384626433832795028841971])。
2. 而第一個 page 在一開始就有 mapping 是因為 allocator 會在第一個page維護 chunk metadata。如下圖 :
![image](https://hackmd.io/_uploads/r1G5roBlbx.png)



## Question 2
<details>
    <summary>Description</summary>
    
- In this part, you need to convert a C system call into a shared library so that it can be called from Python using `ctypes` library.
```c=
//  my_get_phy.c
#include <unistd.h>

#define SYS_hello XXX // 你設定的 syscall number

// Python 會用 ctypes 呼叫這個函數
unsigned long my_get_physical_addresses(void *virtual_addr) {
    return (unsigned long)syscall(SYS_hello, virtual_addr);
}

```
- Compile the C code into a shared library using:
```c
gcc -Wall -O2 -fPIC -shared -o lib_my_get_phy.so my_get_phy.c
```
- All programming languages are eventually translated into machine code that runs on the operating system (OS).
- Each program, regardless of its language, is executed as a process managed by the OS.
- Therefore, all processes - whether written in C, Python, or other languages have their own heap, stack, and page table, just like a normal C program.
- We use Python to observe the heap growth with `sbrk()`.
- After allocating more heap memory, you can call your system call to check the physical addresses of the new memory.
- Some virtual pages may not have a physical mapping, physical memory is only assigned when the pages are accessed.
</details>

### ==Python code (Test Code)==
```python
import ctypes

# -----------------------------
# C syscall wrapper
# -----------------------------
lib = ctypes.CDLL('./lib_my_get_phy.so')
lib.my_get_physical_addresses.argtypes = [ctypes.c_void_p]
lib.my_get_physical_addresses.restype = ctypes.c_ulong

def virt2phys(addr):
    """VA -> PA"""
    return lib.my_get_physical_addresses(ctypes.c_void_p(addr))

# -----------------------------
# sbrk(0) break
# -----------------------------
libc = ctypes.CDLL("libc.so.6")
libc.sbrk.restype = ctypes.c_void_p
libc.sbrk.argtypes = [ctypes.c_long]

def get_heap_break():
    return libc.sbrk(0)

PAGE_SIZE = 4096

print("=== 初始 program break ===")
break_before = get_heap_break()
print(f"program break: {hex(break_before)}\n")

malloc_size = 4096  # 每次 malloc 4KB
buffers = []

# 循環 malloc 觀察 break 變化
for i in range(50):
    buf = ctypes.create_string_buffer(malloc_size)
    buffers.append(buf)
    break_now = get_heap_break()

    # 代表 heap 變大了
    if break_now != break_before:
        print(f"malloc {i+1}: break = {hex(break_now)}")
        print(f"  -> program break 增加了 {break_now - break_before} bytes")

        # 列出新增 break 範圍的 VA -> PA
        start_va = break_before
        end_va = break_now
        print("\n=== 新增 heap VA->PA ===")
        for va in range(start_va, end_va, PAGE_SIZE):
            try:
                pa = virt2phys(va)
                if pa == 0:
                    pa_str = "未分配"
                else:
                    pa_str = f"0x{pa:x}"  # 十六進位格式
            except Exception:
                pa_str = "未分配"
            print(f"VA: 0x{va:x} -> PA: {pa_str}")
        print()
        break_before = break_now
```
### Result
![image](https://hackmd.io/_uploads/HknZ23reZx.png)
1. malloc 會先確認現有的 heap vma areas 是否有足夠的空間分配請求，若是足夠則直接分配 program_break 不會變動，若是不夠則呼叫 sbrk() 擴張一定大小的 heap vma areas，然後再分配，而超出需求的sbrk()大小，可以減少呼叫 syscall 的次數。
2. 可以從執行結果看出 process 開始跑迴圈時 heap vma areas 還有 25 個 free pages 可被分配，到第 26 個 malloc 時因為位置不夠所以呼叫 sbrk()擴展 heap vma areas，而擴展的大小是滿足要求後保有 32 個可分配的 pages，留給之後使用。


## Kernel 的替換 & syscall 的添加 (複習用)
### 1. 前置工作
#### 1.1 下載kernel並解壓縮
以kernel 5.15.137為例
輸入指令後他會下載到當前CMD所處資料夾
```shell
# 下載kernel
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.137.tar.xz
    
# 解壓縮至/usr/src 目錄下
tar -xvf linux-5.15.137.tar.xz -C /usr/src
```
#### 1.2 安裝Compile工具
```shell
# 更新本地的軟體包數據庫，使系統獲取最新的軟體包信息
sudo apt update

# 檢查本地已安裝的軟體包，並嘗試將它們升級到軟體源中可用的最新版本。
sudo apt upgrade

# build-essential：包含基本編譯工具
# libncurses-dev：用於控制終端窗口顯示的應用程式
# libssl-dev：用於開發需要進行加密或使用安全通訊協議（如 HTTPS）的應用程式
# libelf-dev：用於與 ELF 文件格式相關的開發，比如分析或處理可執行文件、目標文件和共享庫
# bison 和 flex：語法解析器生成器 and 詞法分析器生成器
sudo apt install build-essential libncurses-dev libssl-dev libelf-dev bison flex -y

# 安裝vim
sudo apt install vim -y

# 清除安裝的package
sudo apt clean && sudo apt autoremove -y
```
### 2. Syscall 添加以及檔案建置
#### 2.1 建立資料夾
```shell
# 把目錄轉到剛剛解壓縮完的 kernel 檔案夾
cd /usr/src/linux-5.15.137

# 在裡面創建一個名叫 hello 的資料夾
mkdir hello

# 把目錄轉到 hello 資料夾
cd hello
```
#### 2.2 建立一個system call
建立 hello.c
```shell
vim hello.c
```
hello.c 的內容
```c
#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(hello){
    printk("Hello world!\n");
    return 0;
}
```
#### 2.3 在hello資料夾中建立Makefile
建立 Makefile
```shell
vim Makefile
```
Makefile 的內容
```
obj-y := hello.o #將hello.o編入kernel
```
#### 2.4 編輯原系統的Makefile
```shell
# 回上個目錄
cd ..

# 打開此目錄下的 Makefile
vim Makefile
```
vim編輯可以用 ?core-y 來找到core-y這個關鍵字
找到這行如下
```
core-y += kernel/ certs/ mm/ fs/ ipc/ security/ crypto/
```
在最後面補上 hello 這樣 kernel 在編譯時才會到 hello 目錄如下
```
core-y += kernel/ certs/ mm/ fs/ ipc/ security/ crypto/ hello/
```
#### 2.5 將syscall加入到kernel的syscall table
```
vim arch/x86/entry/syscalls/syscall_64.tbl
```
在最後一行添加上 system call，然後請把編號記住等一下會用
![image](https://hackmd.io/_uploads/H1XL8dnJWe.png)
#### 2.6 將系統呼叫對應的函數加入到系統呼叫的標頭檔中
```
cd /usr/src/linux-5.15.137

vim include/linux/syscalls.h

#加入一行(這行直接加在檔案的最下面就好) 用vim可以按 shift+G 就會跳到最後一行
asmlinkage long sys_hello(void);
```
![image](https://hackmd.io/_uploads/Hkukvdhkbx.png)
### 3. 編譯和替換kernel
#### 3.1 清理kernel之前的所有配置
清理上次編譯時產生的目標檔、暫存檔、模組檔 …，以及核心組態檔和備份檔
```shell
sudo make mrproper
```
#### 3.2 編譯設定
進入 kernel 目錄中，並將目前 Kernel Config 文件複製到當前目錄，最後生成此 kernel 的配置文件，為了避免建置大量不必要的 driver 和 kernel module，使用 `localmodconfig` 來節省時間。
```shell
cd linux-5.15.135/
    
cp -v /boot/config-$(uname -r) .config
    
make localmodconfig
```
因為我們直接從 `/boot/config-$(uname -r)` 複製設定檔，所以設定檔裡面的設定的是 Debian 官方當初編譯 kernel 時憑證的路徑，若是直接編譯會報錯，因此這邊取消使用憑證，並將值設為空字串
執行下列命令：
```
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
scripts/config --set-str CONFIG_SYSTEM_REVOCATION_KEYS ""
```
#### 3.3 開始編譯
```shell
make -j12
```
可依據系統核心數改變此指令後面的數字，像這邊12指的是用12顆核心下去跑。
這個步驟會執行得有點久，會等一段時間
或是直接使用以下指令 (查看有多少邏輯核心，並編譯核心)
```
make -j$(nproc)
```
#### 3.4 準備kernel的安裝程式
```
sudo make modules_install -j12
```
#### 3.5 安裝kernel
```
sudo make install -j12
```
#### 3.7 重新啟動
```shell
# 更新作業系統的bootloader成新的kernel
sudo update-grub

# 重新開機
reboot

# 重開後檢查kernel版本
uname -r
```
### 4. 出現問題解法
* 問AI
* [安裝了核心重啟後核心版本沒有更新調處grub引導介面](https://blog.csdn.net/keke_Memory/article/details/124947773)
* [Kernel 的替換 & syscall 的添加](https://satin-eyebrow-f76.notion.site/Kernel-syscall-3ec38210bb1f4d289850c549def29f9f)
* [add a system call to kernel (v5.15.137)](https://hackmd.io/aist49C9R46-vaBIlP3LDA?view=#add-a-system-call-to-kernel-v515137)