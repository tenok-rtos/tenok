import numpy as np
import serial
import struct
import time

from collections import deque
from datetime import datetime

from .yaml_loader import TenokMsgManager

START_BYTE = '@'
CHECKSUM_INIT_VAL = 63

HEADER_SIZE = 3

START_BYTE_POS = 0
SIZE_POS = 1
MSG_ID_POS = 2
PAYLOAD_POS = 3


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


class CSVSaver:
    def __init__(self, file_name, msg_id):
        self.file_name = file_name
        self.msg_id = msg_id
        self.fd = open(file_name, "w")

    def save(self, data_list):
        for i in range(0, len(data_list)):
            data_str = "{:.7f}".format(float(data_list[i]))

            if i == (len(data_list) - 1):
                self.fd.write(data_str)
                self.fd.write('\n')
            else:
                self.fd.write(data_str)
                self.fd.write(',')

            self.fd.flush()

    def close(self):
        self.fd.close()


class SerialManager:
    def __init__(self, port_name, baudrate, msg_manager):
        # Open the serial port
        self.ser = serial.Serial(
            port_name,
            baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=0.1)  # Timeout = 0.1 second
        self.ser.flush()
        self.serial_fifo = deque()
        print("connected to: " + self.ser.portstr)

        self.recvd_time_last = [time.time()
                                for i in range(0, len(msg_manager.msg_list))]
        self.update_rate_last = 0
        self.msg_manager = msg_manager

    def close(self):
        self.ser.close()

    def parse_bool(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        bool_data = struct.unpack("?", binary_data)
        # print("payload #%d: %d" % (i, bool_data))
        return bool_data[0]

    def parse_uint8(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        uint8_data = struct.unpack("H", binary_data)
        # print("payload #%d: %d" % (i, uint8_data))
        return uint8_data[0]

    def parse_int8(self, buffer, i):
        binary_data = struct.pack("B", buffer[i*4])
        int8_data = struct.unpack("h", binary_data)
        # print("payload #%d: %d" % (i, int8_data))
        return int8_data[0]

    def parse_uint16(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        binary_data = data1 + data2
        uint16_data = struct.unpack("H", binary_data)
        # print("payload #%d: %d" % (i, uint16_data))
        return uint16_data[0]

    def parse_int32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        binary_data = data1 + data2
        int16_data = struct.unpack("h", binary_data)
        # print("payload #%d: %d" % (i, int16_data))
        return int16_data[0]

    def parse_uint32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        uint32_data = struct.unpack("I", binary_data)
        # print("payload #%d: %d" % (i, uint32_data))
        return uint32_data[0]

    def parse_int32(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        int32_data = struct.unpack("i", binary_data)
        # print("payload #%d: %d" % (i, int32_data))
        return int32_data[0]

    def parse_float(self, buffer, i):
        data1 = struct.pack("B", buffer[i*4])
        data2 = struct.pack("B", buffer[i*4+1])
        data3 = struct.pack("B", buffer[i*4+2])
        data4 = struct.pack("B", buffer[i*4+3])
        binary_data = data1 + data2 + data3 + data4
        float_data = struct.unpack("f", binary_data)
        # print("payload #%d: %f" % (i, float_data))
        return float_data[0]

    def parse_double(self, buffer, i):
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
        # print("payload #%d: %f" % (i, double_data))
        return double_data[0]

    def decode_field(self, buffer, c_type, i):
        try:
            if c_type == 'bool':
                return self.parse_bool(buffer, i)
            elif c_type == 'uint8_t':
                return self.parse_uint8(buffer, i)
            elif c_type == 'int8_t':
                return self.parse_int8(buffer, i)
            elif c_type == 'uint16_t':
                return self.parse_uint16(buffer, i)
            elif c_type == 'int16_t':
                return self.parse_int16(buffer, i)
            elif c_type == 'uint32_t':
                return self.parse_uint32(buffer, i)
            elif c_type == 'int32_t':
                return self.parse_int32(buffer, i)
            elif c_type == 'uint64_t':
                return self.parse_uint64(buffer, i)
            elif c_type == 'int64_t':
                return self.parse_int64(buffer, i)
            elif c_type == 'float':
                return self.parse_float(buffer, i)
            elif c_type == 'double':
                return self.parse_double(buffer, i)
        except:
            return None

    def prompt(self, msg_name, msg_id):
        print_str = ''

        curr_time = time.time()
        update_rate = 1.0 / (curr_time - self.recvd_time_last[msg_id])
        self.recvd_time_last[msg_id] = curr_time

        # If the frequency change is too rapidly, then it means the communication is unstable
        if abs(update_rate - self.update_rate_last) > 50:
            print_str = '[{}] received \'{}\' (message, id={})\n'.format(
                datetime.now().strftime('%H:%M:%S'), msg_name, msg_id)
            print_str = print_str + "telemetry speed is too slow, the update rate is unknown!\n"
        else:
            print_str = '[{}] received \'{}\' message (id={}, rate={:.1f}Hz)\n'.format(
                datetime.now().strftime('%H:%M:%S'), msg_name, msg_id, update_rate)

        self.update_rate_last = update_rate

        return print_str

    def decode_msg(self, buffer, msg_id):
        msg_info = self.msg_manager.find_id(msg_id)

        # Message type does not exist
        if msg_info is None:
            return 'failed', None, None, None

        msg_name = msg_info.name
        var_cnt = len(msg_info.fields)

        print_str = self.prompt(msg_name, msg_id)

        data_list = []
        recept_cnt = 0

        for i in range(0, var_cnt):
            var_name = msg_info.fields[i].var_name
            array_size = msg_info.fields[i].array_size
            c_type = msg_info.fields[i].c_type

            # The field is a variable if the array_size is set zero
            is_array = True
            if array_size == 0:
                is_array = False
                array_size = 1

            # Decode array elements of each fields
            for j in range(0, array_size):
                decoded_data = self.decode_field(buffer, c_type, recept_cnt)
                if decoded_data is None:
                    return 'failed', None, None, None

                data_list.append(decoded_data)
                recept_cnt = recept_cnt + 1

                if is_array == True:
                    # Print array
                    print_str = print_str + \
                        '- {}[{}]: {}\n'.format(var_name, j, decoded_data)
                else:
                    # Print variable
                    print_str = print_str + \
                        '- {}: {}\n'.format(var_name, decoded_data)

        # Print prompt message
        print(print_str, end='')

        return 'success', msg_name, data_list

    def receive_msg(self):
        # Collect bytes from serial
        avail_cnt = self.ser.inWaiting()
        if avail_cnt > 0:
            for i in range(0, avail_cnt):
                self.serial_fifo.append(self.ser.read(1))

        # Wait until at least the size of header is received
        if len(self.serial_fifo) < HEADER_SIZE:
            return 'retry', None, None, None

        # Start byte checking
        start_byte = struct.unpack("B", self.serial_fifo[START_BYTE_POS])[0]
        if start_byte != ord(START_BYTE):
            # Move to next position by discarding the oldest byte
            self.serial_fifo.pop()
            return 'retry', None, None, None
        #print("received start byte")

        # Get payload size
        payload_cnt = struct.unpack("B", self.serial_fifo[SIZE_POS])[0]
        #print('payload size: %d' %(payload_cnt))

        # Get message ID
        msg_id = struct.unpack("B", self.serial_fifo[MSG_ID_POS])[0]
        #print('message id: %d' %(msg_id))

        # Get payloads and checksum
        if len(self.serial_fifo) < payload_cnt + HEADER_SIZE + 1:
            return 'retry', None, None, None
        else:
            buf = bytearray()
            for i in range(0, payload_cnt + 1):
                buf.extend(self.serial_fifo[i + PAYLOAD_POS])

        # Checksum verification
        checksum = CHECKSUM_INIT_VAL
        for i in range(0, payload_cnt):
            checksum ^= buf[i]

        received_checksum = buf[payload_cnt]
        if received_checksum != checksum:
            # Shift the FIFO by discarding the oldest byte
            self.serial_fifo.pop()
            print("error: checksum mismatched")
            return 'failed', None, None, None

        # Decode message fields
        result, msg_name, data = self.decode_msg(buf, msg_id)
        if result == 'failed':
            return 'failed', None, None, None

        # Discard used bytes from serial buffer
        for i in range(0, payload_cnt + HEADER_SIZE + 1):
            self.serial_fifo.pop()

        return 'success', msg_id, msg_name, data
