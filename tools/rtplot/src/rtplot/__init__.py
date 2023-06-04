from .app_window import RTPlotWindow
from .yaml_loader import TenokMsgLoader
import os
import re
import glob

import serial


def serial_ports():
    ports = glob.glob('/dev/tty[A-Za-z]*')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


def main(msg_dir_path):
    # list yaml files under the input path
    msg_files = glob.glob("%s/tenok_*_msg.yaml" % (msg_dir_path))
    msg_arr = []

    msg_loader = TenokMsgLoader()

    # get all usable messages
    for i in range(0, len(msg_files)):
        # use regex to extract msg name from the file name
        msg_file = os.path.basename(msg_files[i])
        msg_name = re.findall(r'tenok_(.*)_msg.yaml', msg_file)
        msg_arr = msg_arr + msg_name

        # load msg yaml file
        msg_loader.load(msg_files[i])

    if len(msg_arr) == 0:
        print("[rtplot] error, no message is found under %s" % (msg_dir_path))
        exit(-1)

    # get all available serial ports
    ports = serial_ports()

    RTPlotWindow.start_window(ports, msg_arr, msg_loader.msg_list)
