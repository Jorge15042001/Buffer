
import matplotlib.pyplot as plt
import numpy as np


def is_number(val: str):
    if val.isdigit():
        return True
    segs = val.split(".")
    if len(segs) != 2:
        return False
    if not segs[0].isdigit():
        return False
    if not segs[1].isdigit():
        return False
    return True


#  figure, axis = plt.subplots(1, 2)
fig = plt.figure()
ax = fig.add_subplot(111, label="1")
ax2 = fig.add_subplot(111, label="2", frame_on=False)

log_file = open("../a.log")
#  log_file = open("../stats/cb_100_100x1000_200fps.log")

frame_idx = []
free_mem = []
frame_time = []
elapse_time = []
for line in log_file.readlines():
    line = line.strip("\n")
    words = line.split()

    if len(words) != 4:
        continue
    try:

        #  print(list(map(lambda x: is_number(x), words)))
        if (not all(map(lambda x: is_number(x), words))): continue
        frame_idx.append(int(words[0]))
        free_mem.append(float(words[1]))
        frame_time.append(float(words[2]))
        elapse_time.append(float(words[3]))
    except Exception as e:
        print(e)
        continue

frame_idx = np.array(frame_idx)
free_mem= np.array(free_mem)
frame_time = np.array(frame_time)
elapse_time = np.array(elapse_time)

fps = 1/frame_time

ax.plot(frame_idx, free_mem, color="C0")
ax2.plot(frame_idx, elapse_time, color="C1")
#  ax2.xaxis.tick_top()
ax2.yaxis.tick_right()
plt.show()
