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
from PyQt5.QtWidgets import (QApplication, QWidget, QComboBox,
                             QHBoxLayout, QVBoxLayout, QStyle, QLabel, QStatusBar, QTabWidget, QScrollArea)

from .yaml_loader import TenokMsgManager
from .yaml_loader import TenokMsg
from .serial import DataQueue


class MyCanvas(FigureCanvas):
    def __init__(self, width, height):
        # reference: https://stackoverflow.com/questions/71898494/weird-behaviour-with-qscrollarea-in-pyqt5-not-scrolling
        self.fig = Figure(figsize=(5, 4), tight_layout=True)
        FigureCanvas.__init__(self, self.fig)
        FigureCanvas.setMinimumSize(self, QtCore.QSize(width, height))


class RTPlotWindow(QtWidgets.QMainWindow):
    def __init__(self, ports, msg_list, msg_manager):
        self.serial_ports = ports
        self.msg_list = msg_list
        self.msg_manager = msg_manager
        self.curr_msg_info = None
        self.display_off = True
        self.serial_state = "disconnected"
        self.plot_pause = False
        self.redraw_time = 0
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

        self.app_start_time = time.time()

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

        combo_ports = QComboBox(self._main)
        combo_ports.addItems(self.serial_ports)
        combo_ports.setFixedSize(combo_ports.sizeHint())
        hbox_topbar.addWidget(combo_ports)

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
        # stop feeding test data
        self.timer.stop()

        # stop animation before cleaning
        for i in range(0, self.subplot_cnt):
            self.matplot_ani[i].event_source.stop()

        # delete of pyqt objects requires only the toppest object
        sip.delete(self.tab)  # this cleans the nav bar, canvas, etc.
        sip.delete(self.timer)

        del self.signal
        del self.matplot_ani

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

        return ret_signal

    def update_plots(self):
        if self.plot_pause == True:
            return

        # generate fake signal
        speed = 0.1
        for i in range(0, len(self.signal)):
            t = time.time() - self.app_start_time

            val = math.sin(speed * (i + 1) * t)
            self.data_list[i].add(val)
            self.time_list[i].add(t)

        # force redraw every 30Hz period
        redraw_freq = 30
        redraw_period = 1.0 / redraw_freq
        curr_time = time.time()

        # check redraw timer
        if (curr_time - self.redraw_time) > redraw_period:
            # scroll the time axis
            if (curr_time - self.app_start_time) > self.x_axis_len:
                self.x_axis_min = t - self.x_axis_len
                self.x_axis_max = t

                new_x_lim = [self.x_axis_min, self.x_axis_max]
                for i in range(0, self.subplot_cnt):
                    self.subplot[i].set_xlim(new_x_lim)

            # asjust y range limits
            for i in range(0, self.subplot_cnt):
                self.subplot[i].relim()
                self.subplot[i].autoscale_view()

            # redraw
            canvas_index = self.tab.currentIndex()
            self.matplot_canvas[canvas_index].draw()

            # update redraw time
            self.redraw_time = curr_time

    def btn_connect_clicked(self):
        if self.serial_state == "disconnected":
            self.serial_state = "connected"
            self.btn_connect.setText('Disconnect')
            self.checkbox_csv.setEnabled(False)
            self.btn_pause.setEnabled(True)
        elif self.serial_state == "connected":
            self.serial_state = "disconnected"
            self.btn_connect.setText('Connect')
            self.checkbox_csv.setEnabled(True)
            self.btn_pause.setEnabled(False)

    def combo_msgs_activated(self):
        curr_selected_msg = self.combo_msgs.currentText()

        if curr_selected_msg == "---message---" or curr_selected_msg == self.old_selected_msg:
            return  # ignore

        self.old_selected_msg = curr_selected_msg

        # delete old plots
        if self.display_off == False:
            self.delete_plots()

        # load message information
        selected_msg = self.combo_msgs.currentText()
        self.curr_msg_info = self.msg_manager.find(selected_msg)

        self.subplot_cnt = len(self.curr_msg_info.fields)

        # resize window
        self.resize(700, 600)

        self.tab = QTabWidget()
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
                self.update, who=i), data_x_range, interval=0, blit=True)
            self.matplot_ani.append(animator)

        self.layout_main.addWidget(self.tab)

        # setup timer for displaying test data
        self.timer = QtCore.QTimer()
        self.timer.setInterval(1)
        self.timer.timeout.connect(self.update_plots)
        self.timer.start()

        self.display_off = False

    def btn_pause_clicked(self):
        if self.plot_pause == False:
            self.plot_pause = True
            pixmapi = getattr(QStyle, 'SP_MediaPlay')
            icon = self.style().standardIcon(pixmapi)
            self.btn_pause.setIcon(icon)
        elif self.plot_pause == True:
            self.plot_pause = False
            pixmapi = getattr(QStyle, 'SP_MediaPause')
            icon = self.style().standardIcon(pixmapi)
            self.btn_pause.setIcon(icon)

    def start_window(serial_ports, msg_list, msg_manager: TenokMsgManager):
        qapp = QtWidgets.QApplication.instance()
        if not qapp:
            qapp = QtWidgets.QApplication(sys.argv)

        app = RTPlotWindow(serial_ports, msg_list, msg_manager)
        app.show()
        app.activateWindow()
        app.raise_()
        qapp.exec()
