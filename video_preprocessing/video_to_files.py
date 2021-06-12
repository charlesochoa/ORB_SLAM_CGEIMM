# Create the folder with the images extracted from the video

import cv2, time, sys, os, imutils


from datetime  import datetime
from cv_bridge import CvBridge


def create_video_images(video_path, img_folder):
    print('Capturing video...')

    capture = cv2.VideoCapture(video_path)     
    cb      = CvBridge()

    fps = int(capture.get(cv2.CAP_PROP_FPS)) # get the fps of the videofile
    if fps != fps or fps <= 1e-2:
        print("Warning: can't get fps")
    else:
        print('fps achieved succesfull: fps = {}'.format(fps))

    # Open a Window to show the prossesed images in
    cv2.startWindowThread()
    cv2.namedWindow('img', cv2.WINDOW_NORMAL)

    print('Starting creating imgs..')
    # Indicates if there are new frames available
    new_frame_available = True 
    frame_id = 0

    # Loop through all the frames 
    while (new_frame_available):
        new_frame_available, frame = capture.read()

        if not new_frame_available: 
            break 

        # There are new frames
        frame_id += 1 # Create a new frame id
        # timestamp = frame_id*1/fps
        pid = '0'*(6 - len(str(frame_id))) + str(frame_id)

        imagename = img_folder + '/{}.jpg'.format(pid)
        # frame     = cv2.resize(frame,None,fx=0.8,fy=0.8)
        written   = cv2.imwrite(imagename , frame)
        if not written:
            print('Writing frame number {}'.format(frame_id))

        cv2.imshow('img',frame) # Show the prossesed frame
        cv2.waitKey(1)

    # All frames have been prossesed
    print('Done creating images')
    cv2.destroyAllWindows() # Close the window
    capture.release()       # Close the capture video
    

if len(sys.argv) < 3: 
    print('Please supply two arguments as following: python3 video_to_files.py <videopath> <imagefolder>')
    exit(1)

video_path = sys.argv[1]
img_folder = sys.argv[2]

if not os.path.exists(img_folder):
    os.makedirs(img_folder)

create_video_images(video_path, img_folder)
print('Done')
