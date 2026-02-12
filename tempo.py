import time

inicio = time.perf_counter()

x = 0
while x < 1000000:
    x = x + 1

fim = time.perf_counter()

print((fim - inicio) * 1000, "ms")
