#!/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

logs = 0
with open('logs') as f:
    logs = f.readlines()

data = [{} for _ in range(0, 10)]

for line in logs:
    line = line.split()
    tick, pid, q = line
    pid = int(pid) - 4
    data[pid][tick] = int(q)

#print(list(data[0].values()))

#x = [[] for _ in range(10)]
#y = [[] for _ in range(10)]


marks = [".", ",", "o", "v", "^", "<", ">", "8", "s",
         "p", "P", "*", "h", "H", "+", "x", "X", "D", "d"]

#for i in range(0, 10):
#    for tick in sorted(data[i]):
#        x[i].append(tick)
#        y[i].append(data[i][tick])
        

#for i in range(0, 10):
#    plt.plot(y[i], label=i+4, marker=marks[i])

for pid in range(0,10):
    plt.plot(list(data[pid].values()), linestyle='-', marker='o', label = str(pid+4))

plt.yticks([0,1,2,3,4])
plt.xlabel('ticks')
plt.ylabel('queue')
#plt.xticks(np.arange(0, 2100, 100))
plt.legend()
plt.show()
