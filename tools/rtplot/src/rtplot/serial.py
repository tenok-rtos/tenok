import numpy as np
import serial
import struct
import time

from collections import deque
from datetime import datetime

from .yaml_loader import TenokMsgManager

# options
save_csv = False
csv_file = 'serial_log.csv'


class DataQueue:
    def __init__(self, max_count):
        self.max_count = max_count
        self.data = deque([0.0] * max_count)

    def add(self, value):
        if len(self.data) < self.max_count:
            self.data.append(val)
        else:
            self.data.pop()
            self.data.appendleft(value)


class SerialManager:
    def __init__(self, port_name, baudrate, msg_manager):
        # open the serial port
        self.ser = serial.Serial(
            port_name,
            baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=100)

        print("connected to: " + self.ser.portstr)

        # open the csv file
        if save_csv == True:
            self.csv_token = open(csv_file, "w")

        self.plot_time_last = time.time()
        self.update_rate_last = 0
        self.recvd_datas = []

        self.msg_manager = msg_manager

    def close(self):
        self.ser.close()

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

    def parse_field_bool(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        bool_data = struct.unpack("?", binary_data)
        #print("payload #%d: %d" % (i, bool_data))
        return bool_data[0]

    def parse_field_uint8(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        uint8_data = struct.unpack("H", binary_data)
        #print("payload #%d: %d" % (i, uint8_data))
        return uint8_data[0]

    def parse_field_int8(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        int8_data = struct.unpack("h", binary_data)
        #print("payload #%d: %d" % (i, int8_data))
        return int8_data[0]

    def parse_field_uint16(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        binary_data = data1 + data2
        uint16_data = struct.unpack("H", binary_data)
        #print("payload #%d: %d" % (i, uint16_data))
        return uint16_data[0]

    def parse_field_int32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        binary_data = data1 + data2
        int16_data = struct.unpack("h", binary_data)
        #print("payload #%d: %d" % (i, int16_data))
        return int16_data[0]

    def parse_field_uint32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        uint32_data = struct.unpack("I", binary_data)
        #print("payload #%d: %d" % (i, uint32_data))
        return uint32_data[0]

    def parse_field_int32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        int32_data = struct.unpack("i", binary_data)
        #print("payload #%d: %d" % (i, int32_data))
        return int32_data[0]

    def parse_field_float(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        float_data = struct.unpack("f", binary_data)
        #print("payload #%d: %f" % (i, float_data))
        return float_data[0]

    def parse_field_double(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        data5 = struct.pack("B", buffer[i*4+4])
        data6 = struct.pack("B", buffer[i*4+5])
        data7 = struct.pack("B", buffer[i*4+6])
        data8 = struct.pack("B", buffer[i*4+7])
        binary_data = data1 + data2 + data3 + data4 + data5 + data6 + data7 + data8
        double_data = struct.unpack("f", binary_data)
        #print("payload #%d: %f" % (i, double_data))
        return double_data[0]

    def decode_field(self, buffer, c_type, i):
        if c_type == 'bool':
            return self.parse_field_bool(buffer, i)
        elif c_type == 'uint8_t':
            return self.parse_field_uint8(buffer, i)
        elif c_type == 'int8_t':
            return self.parse_field_int8(buffer, i)
        elif c_type == 'uint16_t':
            return self.parse_field_uint16(buffer, i)
        elif c_type == 'int16_t':
            return self.parse_field_int16(buffer, i)
        elif c_type == 'uint32_t':
            return self.parse_field_uint32(buffer, i)
        elif c_type == 'int32_t':
            return self.parse_field_int32(buffer, i)
        elif c_type == 'uint64_t':
            return self.parse_field_uint64(buffer, i)
        elif c_type == 'int64_t':
            return self.parse_field_int64(buffer, i)
        elif c_type == 'float':
            return self.parse_field_float(buffer, i)
        elif c_type == 'double':
            return self.parse_field_double(buffer, i)

    def decode_msg(self, buffer, msg_id):
        msg_info = self.msg_manager.find_id(msg_id)
        var_cnt = len(msg_info.fields)
        recept_cnt = 0

        for i in range(0, var_cnt):
            var_name = msg_info.fields[i].var_name
            array_size = msg_info.fields[i].array_size
            c_type = msg_info.fields[i].c_type
            print_str = ''

            is_array = True
            if array_size == 0:
                is_array = False
                array_size = 1

            for j in range(0, array_size):
                decoded_data = self.decode_field(buffer, c_type, recept_cnt)
                recept_cnt = recept_cnt + 1

                if is_array == True:
                    print_str = print_str + '- {}[{}]: {}\n'.format(var_name, j, decoded_data)
                else:
                    print_str = print_str + '- {}: {}\n'.format(var_name, decoded_data)

            print(print_str, end='')

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

        received_checksum = buf[payload_count]

        if received_checksum != checksum:
            # print("error: checksum mismatch")
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

        self.decode_msg(buffer, message_id)

        if save_csv == True:
            self.save_csv(self.recvd_datas)

        # print("-----------------------------");

        return 'success'
