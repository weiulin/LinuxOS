#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define SYS_GET_PHY  450   // <-- 這裡換成自訂義 syscall number


unsigned long my_get_phy(void *va) {
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
       unsigned long pa = my_get_phy((void *)addr + i);
       printf("  VA: %p -> PA: %p\n", (char *)addr + i, (void *)pa);
   }


   printf("\nTouching memory (write 1 byte per page)...\n");
   for (size_t i = 0; i < length; i += 4096) {
       ((char *)addr)[i] = 42;
   }


   printf("\nCheck VA -> PA after access:\n");
   for (size_t i = 0; i < length; i += 4096) {
       unsigned long pa = my_get_phy((void *)addr + i);
       printf("  VA: %p -> PA: %p\n", (char *)addr + i, (void *)pa);
   }


   return 0;
}
