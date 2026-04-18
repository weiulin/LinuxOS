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
