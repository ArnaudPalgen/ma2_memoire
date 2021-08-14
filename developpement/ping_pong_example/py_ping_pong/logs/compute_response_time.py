import numpy as np

file = "lora_root_response_time.txt"

intervals = []

t1=None

with open(file, 'r') as f:
    for line in f:
        if t1 is None:
            t1 = int(line.split(':')[-1])
        else:
            t2= int(line.split(':')[-1])
            intervals.append(t2-t1)
            t1=None
print("intervals: ", intervals)
print("max", np.amax(intervals))
print("min", np.amin(intervals))
print("var", np.var(intervals))
print("moy", np.average(intervals))