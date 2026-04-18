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
SYSCALL_DEFINE1(my_get_phy, void __user *, vaddr)
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
