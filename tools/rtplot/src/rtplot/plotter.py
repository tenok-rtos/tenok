import matplotlib
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import argparse
import threading
import serial
import struct
import time

from collections import deque
from datetime import datetime

# options
serial_port = '/dev/ttyUSB0'
serial_baudrate = 115200
save_csv = False
csv_file = 'serial_log.csv'


class serial_data_class:
    def __init__(self, max_count):
        self.max_count = max_count
        self.data = deque([0.0] * max_count)

    def add(self, value):
        if len(self.data) < self.max_count:
            self.data.append(val)
        else:
            self.data.pop()
        self.data.appendleft(value)


class serial_plotter_class:
    def __init__(self):
        # open the serial port
        self.ser = serial.Serial(
            serial_port,
            serial_baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=100)

        print("connected to: " + self.ser.portstr)

        # open the csv file
        if save_csv == True:
            self.csv_token = open(csv_file, "w")

        # assign color cycle list
        matplotlib.rcParams['axes.prop_cycle'] = \
            matplotlib.cycler(color=["red", "blue", "green",
                                     "orange", "purple", "brown",
                                     "pink", "gray", "cyan"])

        self.figure = plt.figure(figsize=(14, 8))
        self.curve = []
        self.current_curve_count = 0
        self.plot_begin = False
        self.plot_time_last = time.time()
        self.update_rate_last = 0
        self.recvd_datas = []

    def new_curve(self, label_name):
        plot_list = plt.plot(self.serial_data[self.current_curve_count].data,
                             label=label_name, animated=True)

        for i in range(0, len(self.curve_indexs)):
            if self.curve_indexs[i] == self.current_curve_count:
                self.curve.append(plot_list[0])

        self.current_curve_count += 1

    def set_subplot(self):
        plt.grid()
        plt.legend(loc='upper center', ncol=3, fancybox=True, shadow=True,
                   bbox_to_anchor=(0.5, 1.05),)
        plt.gca().set_prop_cycle(None)  # reset color cycle

    def update(self, i):
        # wait until initialization is finished
        while self.plot_begin == False:
            self.serial_receive()

        # receive new data and and parse messages
        self.serial_receive()

        # update new data income from serial receiver to plotter
        for index in range(0, len(self.curve_indexs)):
            new_data = self.serial_data[self.curve_indexs[index]].data
            self.curve[index].set_ydata(new_data)

        return self.curve

    def run(self):
        ani = animation.FuncAnimation(self.figure, self.update, np.arange(0, 200),
                                      interval=0, blit=True)
        plt.show()

    def save_csv(self, datas):
        for i in range(0, len(datas)):
            data_str = "{:.7f}".format(float(datas[i]))

            if i == (self.curve_number - 1):
                self.csv_token.write(data_str)
                self.csv_token.write('\n')
            else:
                self.csv_token.write(data_str)
                self.csv_token.write(',')

            self.csv_token.flush()

    def set_figure(self, message_id):
        plt.subplot(311)
        plt.ylabel('time [ms]')
        plt.ylim([0.0, 500000.0])
        self.new_curve('time')
        self.set_subplot()

        plt.subplot(312)
        plt.ylabel('accel [m/s^2]')
        plt.ylim([-10, 10])
        self.new_curve('a[0]')
        self.new_curve('a[1]')
        self.new_curve('a[2]')
        self.set_subplot()

        plt.subplot(312)
        plt.ylabel('quaternion')
        plt.ylim([-0.1, 1.1])
        self.new_curve('q[0]')
        self.new_curve('q[1]')
        self.new_curve('q[2]')
        self.new_curve('q[3]')
        self.set_subplot()

    def parse_field_float(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        float_data = np.asarray(struct.unpack("f", binary_data))
        self.serial_data[i].add(float_data)
        print("payload #%d: %f" % (i, float_data))
        return float_data

    def parse_field_int32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        int32_data = np.asarray(struct.unpack("i", binary_data))
        self.serial_data[i].add(int32_data)
        self.recvd_datas.append(int32_data)
        print("payload #%d: %d" % (i, int32_data))
        return int32_data

    def parse_test_message(self, buffer):
        self.parse_field_int32(buffer, 0)
        self.parse_field_float(buffer, 1)
        self.parse_field_float(buffer, 2)
        self.parse_field_float(buffer, 3)
        self.parse_field_float(buffer, 4)
        self.parse_field_float(buffer, 5)
        self.parse_field_float(buffer, 6)
        self.parse_field_float(buffer, 7)

    def serial_receive(self):
        buffer = []
        checksum = 0

        c = self.ser.read(1)
        try:
            c = c.decode("ascii")
        except:
            return 'fail'

        # print("c:")
        # print(c)

        # wait for start byte
        if c == '@':
            pass
        else:
            return 'fail'

        # receive package size
        payload_count, =  struct.unpack("B", self.ser.read(1))
        # print('payload size: %d' %(payload_count))

        # receive message id
        _message_id, =  struct.unpack("c", self.ser.read(1))
        message_id = ord(_message_id)

        buf = self.ser.read(payload_count + 1)

        # receive payload and calculate checksum
        for i in range(0, payload_count):
            buffer.append(buf[i])
            buffer_checksum = buffer[i]
            checksum ^= buffer_checksum

        # received_checksum ,= struct.unpack("B", buf[payload_count])
        received_checksum = buf[payload_count]

        if received_checksum != checksum:
            print("error: checksum mismatch")
            return 'fail'
        else:
            # print("checksum is correct (%d)" %(checksum))
            pass

        plot_time_now = time.time()
        update_rate = 1.0 / (plot_time_now - self.plot_time_last)
        self.plot_time_last = plot_time_now

        if abs(update_rate - self.update_rate_last) > 50:
            print('[%s]received message #%d'
                  % (datetime.now().strftime('%H:%M:%S'),
                     message_id))
            print("telemetry speed is too slow, the update rate is unknown!")
        else:
            print('[%s]received message #%d (%.1fHz)'
                  % (datetime.now().strftime('%H:%M:%S'),
                     message_id, update_rate))

        self.update_rate_last = update_rate

        if self.plot_begin == False:
            self.curve_number = payload_count // 4
            self.serial_data = [serial_data_class(200)
                                for i in range(0, self.curve_number)]
            self.curve_indexs = [i for i in range(0, self.curve_number)]
            self.set_figure(message_id)
            self.plot_begin = True

        parse_test_message(buffer)

        if save_csv == True:
            self.save_csv(self.recvd_datas)

        # print("-----------------------------");

        return 'success'


def plotter_start():
    serial_plotter = serial_plotter_class()
    serial_plotter.run()

plotter_start()
