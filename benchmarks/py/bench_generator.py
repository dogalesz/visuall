def evens(n):
    for i in range(n):
        yield i * 2
total = sum(evens(100000))
print(total)
