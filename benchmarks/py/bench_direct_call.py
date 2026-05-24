def compute(x):
    return x * 3 + 1
total = 0
for i in range(1000000):
    total += compute(i)
print(total)
