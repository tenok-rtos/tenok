from .app_window import RTPlotWindow
from .yaml_loader import TenokMsgManager
import os
import re
import glob

import serial
import serial.tools.list_ports


def serial_ports():
    port_list = serial.tools.list_ports.comports()

    ports = ['/dev/' + port.name for port in port_list]
    ports = ports + glob.glob('/dev/pts/[0-9]*')

    return ports


def main(msg_dir_path):
    # List YAML files under the input path
    msg_files = glob.glob("%s/debug_link_*_msg.yaml" % (msg_dir_path))
    msg_arr = []

    msg_manager = TenokMsgManager()

    # Get all usable messages
    for i in range(0, len(msg_files)):
        # Use regex to extract msg name from the file name
        msg_file = os.path.basename(msg_files[i])
        msg_name = re.findall(r'debug_link_(.*)_msg.yaml', msg_file)[0]
        msg_arr = msg_arr + [msg_name]

        # Load msg YAML file
        msg_manager.load(msg_files[i], msg_name)

    if len(msg_arr) == 0:
        print("[rtplot] error, no message is found under %s" % (msg_dir_path))
        exit(-1)

    # Get all available serial ports
    ports = serial_ports()

    RTPlotWindow.start_window(ports, msg_arr, msg_manager)
