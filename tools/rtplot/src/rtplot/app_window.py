import os
import sys
import time
import sip
import math

import numpy as np
import matplotlib.animation as animation

from functools import partial
from matplotlib.backends.qt_compat import QtWidgets
from matplotlib.backends.backend_qtagg import (
    FigureCanvas, NavigationToolbar2QT as NavigationToolbar)
from matplotlib.figure import Figure
from PyQt5 import QtCore
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (QApplication, QWidget, QComboBox,
                             QHBoxLayout, QVBoxLayout, QStyle, QLabel, QStatusBar, QTabWidget, QScrollArea)

from .yaml_loader import TenokMsgManager
from .yaml_loader import TenokMsg
from .serial import DataQueue
from .serial import SerialManager
from .serial import CSVSaver


class QSerialThread(QtCore.QThread):
    data_ready_signal = pyqtSignal(int, str, list)

    def __init__(self, port_name, baudrate, msg_manager):
        QtCore.QThread.__init__(self)
        self.portname = port_name
        self.baudrate = baudrate
        self.serial_manager = SerialManager(port_name, baudrate, msg_manager)

    def run(self):
        self.running = True
        while self.running == True:
            # message reception
            result, msg_id, msg_name, data = self.serial_manager.receive_msg()

            # send the data to the app window
            if result == 'success':
                self.data_ready_signal.emit(msg_id, msg_name, data)

    def stop(self):
        self.running = False
        self.wait()
        self.serial_manager.close()


class MyCanvas(FigureCanvas):
    def __init__(self, width, height):
        # reference: https://stackoverflow.com/questions/71898494/weird-behaviour-with-qscrollarea-in-pyqt5-not-scrolling
        self.fig = Figure(figsize=(5, 4))
        self.fig.tight_layout()
        FigureCanvas.__init__(self, self.fig)
        FigureCanvas.setMinimumSize(self, QtCore.QSize(width, height))

    def resizeEvent(self, event):
        FigureCanvas.resizeEvent(self, event)
        self.fig.tight_layout()


class RTPlotWindow(QtWidgets.QMainWindow):
    def __init__(self, ports, msg_list, msg_manager):
        self.serial_ports = ports
        self.msg_list = msg_list
        self.msg_manager = msg_manager
        self.curr_msg_info = None
        self.display_off = True
        self.serial_state = "disconnected"
        self.plot_pause = False
        self.old_selected_msg = ""

        # plot data
        self.data_list = []  # sensor measurement values
        self.time_list = []  # received time sequence of the rtplot
        self.data_size = 10000
        self.subplot_cnt = 0
        self.curve_cnt = 0

        # display time range
        self.x_axis_len = 20
        self.x_axis_min = 0
        self.x_axis_max = self.x_axis_min + self.x_axis_len

        self.ui_init()

        # serial thread
        self.ser_thread = None

        self.x_start_time = time.time()
        self.x_last_time = self.x_start_time

        # csv saver
        self.csv_saver = []

    def closeEvent(self, event):
        if self.ser_thread != None:
            self.ser_thread.stop()

    def serial_ready_event(self, msg_id, msg_name, serial_data_list):
        # select csv saver
        _csv_saver = None
        for i in range(0, len(self.csv_saver)):
            if self.csv_saver[i].msg_id == msg_id:
                _csv_saver = self.csv_saver[i]
                break

        # save csv
        if _csv_saver != None:
            _csv_saver.save(serial_data_list)

        curr_selected_msg = self.combo_msgs.currentText()
        if curr_selected_msg != msg_name or self.display_off == True:
            return

        for i in range(0, len(self.signal)):
            curr_time = time.time()
            self.x_last_time = curr_time
            t = curr_time - self.x_start_time

            self.data_list[i].add(serial_data_list[i])
            self.time_list[i].add(t)

    def ui_init(self):
        super().__init__()

        self._main = QtWidgets.QWidget()
        self.setCentralWidget(self._main)
        self.setWindowTitle('rtplot')
        self.setMinimumSize(700, 50)
        self.resize(700, 50)

        self.layout_main = QVBoxLayout(self._main)

        #=========#
        # top bar #
        #=========#
        hbox_topbar = QHBoxLayout()

        self.combo_ports = QComboBox(self._main)
        self.combo_ports.addItems(self.serial_ports)
        self.combo_ports.setFixedSize(self.combo_ports.sizeHint())
        hbox_topbar.addWidget(self.combo_ports)

        self.combo_baudrates = QComboBox(self._main)
        self.combo_baudrates.addItems(
            ['9600', '19200', '38400', '57600', '115200'])
        self.combo_baudrates.setFixedSize(self.combo_baudrates.sizeHint())
        hbox_topbar.addWidget(self.combo_baudrates)

        self.btn_connect = QtWidgets.QPushButton(self._main)
        self.btn_connect.setText('Connect')
        self.btn_connect.setFixedSize(self.btn_connect.sizeHint())
        self.btn_connect.clicked.connect(self.btn_connect_clicked)
        hbox_topbar.addWidget(self.btn_connect)

        self.checkbox_csv = QtWidgets.QCheckBox(self._main)
        self.checkbox_csv.setText('Save CSV')
        # checkbox_csv.setFixedSize(checkbox_csv.sizeHint())
        hbox_topbar.addWidget(self.checkbox_csv)

        self.combo_msgs = QComboBox(self._main)
        self.combo_msgs.addItems(['---message---'] + self.msg_list)
        self.combo_msgs.setFixedSize(self.combo_msgs.sizeHint())
        self.combo_msgs.activated.connect(self.combo_msgs_activated)
        hbox_topbar.addWidget(self.combo_msgs)

        self.btn_pause = QtWidgets.QPushButton(self._main)
        self.btn_pause.setFixedSize(self.btn_pause.sizeHint())
        pixmapi = getattr(QStyle, 'SP_MediaPause')
        icon = self.style().standardIcon(pixmapi)
        self.btn_pause.setIcon(icon)
        self.btn_pause.setEnabled(False)
        self.btn_pause.clicked.connect(self.btn_pause_clicked)
        hbox_topbar.addWidget(self.btn_pause)

        self.layout_main.addLayout(hbox_topbar)

    def delete_plots(self):
        # stop animation before deleting canvas
        for i in range(0, self.subplot_cnt):
            # call the private function to force the garbage collection
            # recyle the matplot animation object
            self.matplot_ani[i]._stop()

        # delete of pyqt objects requires only the toppest object
        sip.delete(self.tab)  # this cleans the nav bar, canvas, etc.

        del self.signal
        del self.matplot_ani

        del self.data_list
        del self.time_list

        self.display_off = True

    def update(self, art, *, who):
        start_index = self.subplot_info[who][0]
        array_size = self.subplot_info[who][1]

        curve_cnt = 1 if array_size == 0 else array_size

        ret_signal = []
        for i in range(start_index, start_index + curve_cnt):
            self.signal[i].set_xdata(self.time_list[i].data)
            self.signal[i].set_ydata(self.data_list[i].data)
            ret_signal.append(self.signal[i])

        self.adjust_xy_axis()

        return ret_signal

    def adjust_xy_axis(self):
        if self.plot_pause == True:
            return

        # scroll the x axis (time)
        if (self.x_last_time - self.x_start_time) > self.x_axis_len:
            t = self.x_last_time - self.x_start_time
            self.x_axis_min = t - self.x_axis_len
            self.x_axis_max = t

            new_x_lim = [self.x_axis_min, self.x_axis_max]
            for i in range(0, self.subplot_cnt):
                self.subplot[i].set_xlim(new_x_lim)

        # autoscale of the y axis
        for i in range(0, self.subplot_cnt):
            self.subplot[i].relim()
            self.subplot[i].autoscale_view()

    def btn_connect_clicked(self):
        if self.serial_state == "disconnected":
            self.serial_state = "connected"
            self.btn_connect.setText('Disconnect')
            self.combo_ports.setEnabled(False)
            self.combo_baudrates.setEnabled(False)
            self.checkbox_csv.setEnabled(False)
            self.btn_pause.setEnabled(True)

            # launch the serial thread
            port_name = self.combo_ports.currentText()
            baudrate = int(self.combo_baudrates.currentText())
            self.ser_thread = QSerialThread(
                port_name, baudrate, self.msg_manager)
            self.ser_thread.data_ready_signal.connect(self.serial_ready_event)
            self.ser_thread.start()

            # start the csv saver
            if self.checkbox_csv.isChecked() == True:
                self.csv_saver = []
                msg_cnt = len(self.msg_manager.msg_list)

                # create log directory if it does not exist
                if not os.path.isdir('./logs'):
                    os.mkdir('./logs')

                for i in range(0, msg_cnt):
                    file_name = './logs/{}.log'.format(
                        self.msg_manager.msg_list[i].name)
                    msg_id = self.msg_manager.msg_list[i].msg_id
                    self.csv_saver.append(CSVSaver(file_name, msg_id))

        elif self.serial_state == "connected":
            self.serial_state = "disconnected"
            self.btn_connect.setText('Connect')
            self.combo_ports.setEnabled(True)
            self.combo_baudrates.setEnabled(True)
            self.checkbox_csv.setEnabled(True)
            self.btn_pause.setEnabled(False)

            self.ser_thread.stop()
            del self.ser_thread
            self.ser_thread = None

            # close the csv saver
            if self.checkbox_csv.isChecked() == True:
                msg_cnt = len(self.msg_manager.msg_list)
                for i in range(0, msg_cnt):
                    self.csv_saver[i].close()
                del self.csv_saver
                self.csv_saver = []

    def combo_msgs_activated(self):
        curr_selected_msg = self.combo_msgs.currentText()

        if curr_selected_msg == "---message---" or curr_selected_msg == self.old_selected_msg:
            return  # ignore

        self.old_selected_msg = curr_selected_msg

        # delete old plots
        if self.display_off == False:
            self.delete_plots()

        # reset time
        self.x_start_time = time.time()

        # load message information
        selected_msg = self.combo_msgs.currentText()
        self.curr_msg_info = self.msg_manager.find_name(selected_msg)

        self.subplot_cnt = len(self.curr_msg_info.fields)

        # resize window
        self.resize(700, 600)

        self.tab = QTabWidget()
        self.tab.currentChanged.connect(self.tab_on_change)
        self.plot_container = [QWidget() for i in range(0, self.subplot_cnt)]
        self.plot_layouts = [QVBoxLayout() for i in range(0, self.subplot_cnt)]

        min_width = 500
        min_height = 150
        self.matplot_canvas = []
        self.matplot_nav_bar = []
        self.fig = []
        self.subplot = []
        self.subplot_info = []
        self.matplot_ani = []

        # set up subplots
        self.signal = []
        x_arr = np.linspace(0, self.data_size, self.data_size + 1)
        y_arr = np.zeros(self.data_size + 1)
        self.curve_cnt = 0

        # initialize the tab widget
        for i in range(0, self.subplot_cnt):
            # create plot canvas for each tab
            self.matplot_canvas.append(MyCanvas(min_width, min_height))
            self.matplot_canvas[i].figure.set_facecolor("lightGray")
            self.fig.append(self.matplot_canvas[i].figure)

            # create new subplot
            self.subplot.append(self.fig[i].subplots(1, 1))

            # retrieve message declaration
            y_label = self.curr_msg_info.fields[i].description
            var_name = self.curr_msg_info.fields[i].var_name
            array_size = self.curr_msg_info.fields[i].array_size

            # record curve start index and curve count
            self.subplot_info.append((self.curve_cnt, array_size))

            # check the data to plot is a variable or an array
            if array_size == 0:
                new_signal, = self.subplot[i].plot(
                    x_arr, y_arr, label=var_name)
                self.signal.append(new_signal)
                self.curve_cnt = self.curve_cnt + 1
            elif array_size > 0:
                for j in range(0, array_size):
                    label_text = "%s[%d]" % (var_name, j)
                    new_signal, = self.subplot[i].plot(
                        x_arr, y_arr, label=label_text)
                    self.signal.append(new_signal)
                    self.curve_cnt = self.curve_cnt + 1

            # configure the new subplot
            self.subplot[i].grid(color="lightGray")
            self.subplot[i].set_xlim([0, self.x_axis_max])
            self.subplot[i].set_xlabel("time [s]")
            self.subplot[i].set_ylabel(y_label)
            self.subplot[i].legend(loc='upper left', shadow=True)

            # create navigation bar for each tab
            self.matplot_nav_bar.append(
                NavigationToolbar(self.matplot_canvas[i], self))

            # add each canvas into a new tab
            self.tab.addTab(self.plot_container[i], str(i))
            self.plot_layouts[i].addWidget(self.matplot_nav_bar[i])
            self.plot_layouts[i].addWidget(self.matplot_canvas[i])
            self.plot_container[i].setLayout(self.plot_layouts[i])

        # initialize plot data list
        self.data_list = []
        self.time_list = []
        for i in range(0, self.curve_cnt):
            self.data_list.append(DataQueue(self.data_size + 1))
            self.time_list.append(DataQueue(self.data_size + 1))

        data_x_range = np.arange(0, self.data_size)
        for i in range(0, self.subplot_cnt):
            animator = animation.FuncAnimation(self.fig[i], partial(
                self.update, who=i), data_x_range, interval=10, blit=False)
            self.matplot_ani.append(animator)
            self.matplot_ani[i].event_source.stop()  # disable animation first

            # to sprress the warning of the animation never start when closing the window
            setattr(self.matplot_ani[i], '_draw_was_started', True)

        # enable the animation of canvas on current tab only
        canvas_index = self.tab.currentIndex()
        self.matplot_ani[canvas_index].event_source.start()

        self.layout_main.addWidget(self.tab)

        self.display_off = False

    def tab_on_change(self):
        # ignore the event if the canvas is not yet ready
        if self.display_off == True:
            return

        # disable animation of canvas on every tab
        for i in range(0, self.subplot_cnt):
            self.matplot_ani[i].event_source.stop()

        # enable the animation of canvas on current tab only
        canvas_index = self.tab.currentIndex()
        self.matplot_ani[canvas_index].event_source.start()
        self.matplot_canvas[canvas_index].draw()

    def btn_pause_clicked(self):
        # ignore the event if the canvas is not yet ready
        if self.display_off == True:
            return

        if self.plot_pause == False:
            self.plot_pause = True
            pixmapi = getattr(QStyle, 'SP_MediaPlay')
            icon = self.style().standardIcon(pixmapi)
            self.btn_pause.setIcon(icon)

            # disable the animation of the current matplot canvas
            curr_canvas = self.tab.currentIndex()
            self.matplot_ani[curr_canvas].event_source.stop()

        elif self.plot_pause == True:
            self.plot_pause = False
            pixmapi = getattr(QStyle, 'SP_MediaPause')
            icon = self.style().standardIcon(pixmapi)
            self.btn_pause.setIcon(icon)

            # enable the animation of the current matplot canvas
            canvas_index = self.tab.currentIndex()
            self.matplot_ani[canvas_index].event_source.start()

    def start_window(serial_ports, msg_list, msg_manager: TenokMsgManager):
        qapp = QtWidgets.QApplication.instance()
        if not qapp:
            qapp = QtWidgets.QApplication(sys.argv)

        app = RTPlotWindow(serial_ports, msg_list, msg_manager)
        app.show()
        app.activateWindow()
        app.raise_()
        qapp.exec()
