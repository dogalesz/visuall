total = 0
for i in range(1000000):
    match i % 10:
        case 0: total += 1
        case 1: total += 2
        case 2: total += 3
        case 3: total += 4
        case 4: total += 5
        case 5: total += 6
        case 6: total += 7
        case 7: total += 8
        case 8: total += 9
        case _: total += 10
print(total)
