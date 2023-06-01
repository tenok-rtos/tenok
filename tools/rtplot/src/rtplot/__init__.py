from .app_window import RTPlotWindow
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

    # get all usable messages
    for i in range(0, len(msg_files)):
        msg_file = os.path.basename(msg_files[i])
        msg_name = re.findall(r'tenok_(.*)_msg.yaml', msg_file)
        msg_arr = msg_arr + msg_name

    if len(msg_arr) == 0:
        print("[rtplot] error, no message is found under %s" % (msg_dir_path))
        exit(-1)

    # git all available serial ports
    ports = serial_ports()

    RTPlotWindow.start_window(ports, msg_arr)
