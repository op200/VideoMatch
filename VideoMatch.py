import cv2
import numpy as np
from skimage.metrics import structural_similarity as ssim

import sys
import os

PROGRAM_NAME = "Video Match"
VERSION = "0.1"
HOME_LINK = "https://github.com/op200/VideoMatch"

class log:
    log_level = 0

    @staticmethod
    def output(info:object):
        print(info)

    @staticmethod
    def exit():
        log.error("program interruption")
        exit()

    @staticmethod
    def error(info:object, level:int=110):
        if level>log.log_level:
            log.output(f"\033[31m[ERROR]\033[0m {info}")

    @staticmethod
    def errore(info:object):
        log.error(info)
        log.exit()

    @staticmethod
    def warning(info:object, level:int=70):
        if level>log.log_level:
            log.output(f"\033[33m[WARNING]\033[0m {info}")

    @staticmethod
    def info(info:object, level:int=30):
        if level>log.log_level:
            log.output(f"\033[35m[INFO]\033[0m {info}")

    @staticmethod
    def change_title(info:str):
        os.system(f"title {info}")



class option:
    cmds = sys.argv[1:]

    input_video_path_1 : str = "empty_input_video_path_1"
    input_video_path_2 : str = "empty_input_video_path_2"

    output_type : str = "framenum"

    log_path : str = None

    help_info = f"""{PROGRAM_NAME} v{VERSION} help:
-h/-help
    print help

-v/-version
    print version

-i1/-input1 <string>
    input the path of the first video

-i2/-input2 <string>
    input the path of the second video

-t/-type <string>
    set the output type
    nooutput: no output
    framenum: output the number of matching frames
    default: "{output_type}"

-log <string>
    set the path of log file
    required that -type is not nooutput
    if it is None, no output file
    default: "{log_path}"
"""

    @staticmethod
    def get_option():
        for cmd in option.cmds:
            if cmd=="-v" or cmd=="-version":
                log.output(f"{PROGRAM_NAME}\nVersion: {VERSION}\n{HOME_LINK}")
                exit()
            if cmd=="-h" or cmd=="-help":
                log.output(option.help_info)
                exit()

        for i in range(len(option.cmds)):
            # input 1
            if option.cmds[i]=="-i1" or option.cmds[i]=="-input1":
                option.input_video_path_1 = option.cmds[i+1]

            # input 2
            if option.cmds[i]=="-i2" or option.cmds[i]=="-input2":
                option.input_video_path_2 = option.cmds[i+1]

            # type
            if option.cmds[i]=="-t" or option.cmds[i]=="-type":
                option.output_type = option.cmds[i+1]

            # log
            if option.cmds[i]=="-log":
                option.log_path = option.cmds[i+1]

        # 校验
        if option.output_type=="nooutput":
            option.log_path = None
        
        if not option.output_type in ("nooutput","framenum"):
            log.errore("The -type error")

        global video_cap_1,video_cap_2,frame_count_1,frame_count_2
        video_cap_1 = cv2.VideoCapture(option.input_video_path_1)
        if not video_cap_1.isOpened():
            log.errore(f"{option.input_video_path_1} can not be opened")

        video_cap_2 = cv2.VideoCapture(option.input_video_path_2)
        if not video_cap_2.isOpened():
            log.errore(f"{option.input_video_path_2} can not be opened")

        if video_cap_1.get(cv2.CAP_PROP_FRAME_WIDTH)!=video_cap_2.get(cv2.CAP_PROP_FRAME_WIDTH) or video_cap_1.get(cv2.CAP_PROP_FRAME_HEIGHT)!=video_cap_2.get(cv2.CAP_PROP_FRAME_HEIGHT):
            log.errore("The two videos have different widths or heights")

        frame_count_1 = int(video_cap_1.get(cv2.CAP_PROP_FRAME_COUNT))
        frame_count_2 = int(video_cap_2.get(cv2.CAP_PROP_FRAME_COUNT))

def compare_ssim(frame_1, frame_2):
    ssim_value_list = []
    for i in range(3):
        gray1 = frame_1[...,i]
        gray2 = frame_2[...,i]
        ssim_value_list.append(ssim(gray1,gray2))
    return sum(ssim_value_list) / len(ssim_value_list)
SSIM_THRESHOLD = 0.995
def frame_cmp(frame_1, frame_2) -> bool:
    return compare_ssim(frame_1, frame_2) >= SSIM_THRESHOLD

backward_frame = 24
def do_match():
    global match_frame_list
    match_frame_list = [None]*frame_count_1
    video_frame_num_1, video_frame_num_2 = 0, 0

    while video_frame_num_1 < frame_count_1:
        frame_1 = video_cap_1.read()[1]
        video_cap_2.set(cv2.CAP_PROP_POS_FRAMES, video_frame_num_2)
        for i in range(video_frame_num_2, video_frame_num_2+backward_frame+1):
            frame_2 = video_cap_2.read()[1]
            if frame_cmp(frame_1, frame_2):
                match_frame_list[video_frame_num_1] = i
                video_frame_num_2 = i+1
                break
        else:
            video_frame_num_2 = i
        log.change_title(f"{video_frame_num_1} / {frame_count_1}")
        video_frame_num_1+=1

    video_cap_1.release()
    video_cap_2.release()

def data_output():
    for i in range(len(match_frame_list)):
        print(f"{i}->{match_frame_list[i]}")

    if option.log_path:
        with open(option.log_path, 'w') as log_file:
            for i in range(len(match_frame_list)):
                log_file.write(f"{i}->{match_frame_list[i]}\n")

option.get_option()
do_match()
data_output()
