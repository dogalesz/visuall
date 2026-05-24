class Calc:
    def compute(self, x):
        return x * 3 + 1
    def process(self, x):
        return self.compute(x)
calc = Calc()
total = 0
for i in range(1000000):
    total += calc.process(i)
print(total)
