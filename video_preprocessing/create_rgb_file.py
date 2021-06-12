# Create the rgb.txt file for ORBSLAM
from datetime  import datetime
import sys, os, time


if len(sys.argv) < 3: 
    print('Please supply two arguments as following: python3 create_rgb_file.py <image_folder> <fps>')
    exit(1)

print('Opening textfile...')
filename = 'rgb.txt'
file_    = open(filename,'w')

print('Reading arguments...')
img_folder = sys.argv[1]  
fps        = float(sys.argv[2])

print('Looping over files in {}'.format(img_folder))

i = 0
for _, _, files in os.walk(img_folder):

    files = sorted(files)
    for f in files:
        timestamp = i*1/fps
        new_line  = str(timestamp) + ' ' + img_folder + '/' + f + '\n'
        file_.write(new_line)
        i = i + 1
        file_.close

print('done')
