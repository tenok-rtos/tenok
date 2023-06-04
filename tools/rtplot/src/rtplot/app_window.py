import sys
import time
import sip
import math

import numpy as np
import matplotlib.animation as animation

from matplotlib.backends.qt_compat import QtWidgets
from matplotlib.backends.backend_qtagg import (
    FigureCanvas, NavigationToolbar2QT as NavigationToolbar)
from matplotlib.figure import Figure
from PyQt5 import QtCore
from PyQt5.QtWidgets import (QApplication, QWidget, QComboBox,
                             QHBoxLayout, QStyle, QLabel, QStatusBar, QTabWidget)

from .yaml_loader import TenokMsgManager
from .yaml_loader import TenokMsg
from .serial import serial_data_class


class RTPlotWindow(QtWidgets.QMainWindow):
    def __init__(self, ports, msg_list, msg_manager):
        self.serial_ports = ports
        self.msg_list = msg_list
        self.msg_manager = msg_manager
        self.curr_msg_info = None
        self.display_off = True
        self.serial_state = "disconnected"
        self.plot_pause = False

        # plot data
        self.data_list = []
        self.data_size = 1000

        self.ui_init()

    def resizeEvent(self, event):
        QtWidgets.QMainWindow.resizeEvent(self, event)

        if self.display_off == False:
            self.matplot_canvas.figure.tight_layout()

    def ui_init(self):
        super().__init__()

        self._main = QtWidgets.QWidget()
        self.setCentralWidget(self._main)
        self.setWindowTitle('rtplot')

        self.layout_main = QtWidgets.QVBoxLayout(self._main)

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
        self.matplot_ani.event_source.stop()

        self.matplot_layout.removeWidget(self.matplot_nav_bar)
        self.matplot_layout.removeWidget(self.matplot_canvas)
        self.layout_main.removeWidget(self.tabs)
        sip.delete(self.matplot_nav_bar)
        sip.delete(self.matplot_canvas)
        sip.delete(self.tabs)
        del self._timer
        del self.signal
        del self.matplot_ani
        self.display_off = True

    def update(self, j):
        for i in range(0, len(self.signal)):
            self.signal[i].set_ydata(self.data_list[i].data)

        return self.signal

    def update_plots(self):
        if self.plot_pause == True:
            return

        speed_factor = 10
        for i in range(0, len(self.signal)):
            self.data_list[i].add(
                math.sin(speed_factor * (i + 1) * time.time()))

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
        if self.combo_msgs.currentText() == "---message---":
            return  # ignore

        # load message information
        selected_msg = self.combo_msgs.currentText()
        self.curr_msg_info = self.msg_manager.find(selected_msg)

        subplot_cnt = len(self.curr_msg_info.fields)

        # create plot data list
        self.data_list = [serial_data_class(self.data_size + 1)
                          for i in range(0, subplot_cnt)]

        # delete old plots
        if self.display_off == False:
            self.delete_plots()

        # display plot figures
        self.tabs = QTabWidget()
        self.tab_widgets = [QWidget()]
        self.tabs.addTab(self.tab_widgets[0], "%d" % (1))

        self.matplot_canvas = FigureCanvas(Figure(figsize=(5, 4)))
        self.matplot_canvas.figure.set_facecolor("lightGray")

        self.matplot_layout = QtWidgets.QVBoxLayout(self._main)
        self.matplot_nav_bar = NavigationToolbar(self.matplot_canvas, self)
        self.matplot_layout.addWidget(self.matplot_nav_bar)
        self.matplot_layout.addWidget(self.matplot_canvas)
        self.tab_widgets[0].setLayout(self.matplot_layout)

        x_arr = np.linspace(0, self.data_size, self.data_size + 1)
        y_arr = np.zeros(self.data_size + 1)
        self.signal = []
        fig = self.matplot_canvas.figure
        self._dynamic_ax = fig.subplots(subplot_cnt, 1)
        for i in range(0, subplot_cnt):
            self._dynamic_ax[i].grid(color="lightGray")
            self._dynamic_ax[i].set_xlim([0, self.data_size])
            self._dynamic_ax[i].set_ylim([-1.5, 1.5])
            new_signal, = self._dynamic_ax[i].plot(x_arr, y_arr)
            self.signal.append(new_signal)

        self.matplot_ani = animation.FuncAnimation(fig, self.update,
                                                   np.arange(
                                                       0, self.data_size),
                                                   interval=20, blit=True)

        # create timer for displaying test data
        self._timer = self.matplot_canvas.new_timer(1)
        self._timer.add_callback(self.update_plots)
        self._timer.start()

        self.layout_main.addWidget(self.tabs)

        self.resize(750, 750)
        self.matplot_canvas.figure.tight_layout()

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
        # Check whether there is already a running QApplication (e.g., if running
        # from an IDE).
        qapp = QtWidgets.QApplication.instance()
        if not qapp:
            qapp = QtWidgets.QApplication(sys.argv)

        app = RTPlotWindow(serial_ports, msg_list, msg_manager)
        app.show()
        app.activateWindow()
        app.raise_()
        qapp.exec()
