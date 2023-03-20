import numpy as np
import cv2

vid = open("../buildcpp/file.buf", "rb")
frame_size = w, h = (320, 240)


while True:
    frame_bin = vid.read(w*h)
    if len(frame_bin) == 0:
        break
    frame = np.frombuffer(frame_bin, np.uint8).reshape((h, w))
    #  frame = cv2.equalizeHist(frame)
    frame = cv2.resize(frame, (w*4, h*4))
    #  frame = frame.astype(np.float32)

    #  frame = ((frame-np.min(frame))/(np.max(frame) -
             #  np.min(frame))*255).astype(np.uint8)
    cv2.imshow("video", frame)
    key = cv2.waitKey(2000)
    if key == ord('q'):
        break
