import sys
import time

import numpy as np

from matplotlib.backends.qt_compat import QtWidgets
from matplotlib.backends.backend_qtagg import (
    FigureCanvas, NavigationToolbar2QT as NavigationToolbar)
from matplotlib.figure import Figure
from PyQt5 import QtCore
from PyQt5.QtWidgets import (
    QApplication, QWidget, QComboBox, QHBoxLayout, QStyle, QLabel, QStatusBar)


class RTPlotWindow(QtWidgets.QMainWindow):
    def __init__(self, ports, msg_list):
        self.serial_ports = ports
        self.msg_list = msg_list

        self.serial_state = "disconnected"
        self.plot_pause = False

        self.ui_init()

    def ui_init(self):
        super().__init__()

        self._main = QtWidgets.QWidget()
        self.setCentralWidget(self._main)
        self.setWindowTitle('rtplot')

        layout = QtWidgets.QVBoxLayout(self._main)

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

        combo_msgs = QComboBox(self._main)
        combo_msgs.addItems(['---message---'] + self.msg_list)
        combo_msgs.setFixedSize(combo_msgs.sizeHint())
        hbox_topbar.addWidget(combo_msgs)

        self.btn_pause = QtWidgets.QPushButton(self._main)
        self.btn_pause.setFixedSize(self.btn_pause.sizeHint())
        pixmapi = getattr(QStyle, 'SP_MediaPause')
        icon = self.style().standardIcon(pixmapi)
        self.btn_pause.setIcon(icon)
        self.btn_pause.setEnabled(False)
        self.btn_pause.clicked.connect(self.btn_pause_clicked)
        hbox_topbar.addWidget(self.btn_pause)

        layout.addLayout(hbox_topbar)

        #======#
        # Plot #
        #======#
        matplot_canvas = FigureCanvas(Figure(figsize=(5, 4)))
        matplot_canvas.figure.set_facecolor("lightGray")
        #
        layout.addWidget(NavigationToolbar(matplot_canvas, self))
        layout.addWidget(matplot_canvas)

        t = np.linspace(0, 10, 101)
        #
        self._dynamic_ax = matplot_canvas.figure.subplots(2, 1)
        #
        self._dynamic_ax[0].grid(color="lightGray")
        self._dynamic_ax[0].set_xlim([0, 10])
        self.signal1, = self._dynamic_ax[0].plot(t, np.sin(t + time.time()))
        #
        self._dynamic_ax[1].grid(color="lightGray")
        self._dynamic_ax[1].set_xlim([0, 10])
        self.signal2, = self._dynamic_ax[1].plot(t, np.sin(t + time.time()))

        self._timer = matplot_canvas.new_timer(50)
        self._timer.add_callback(self.update_plots)
        self._timer.start()

    def update_plots(self):
        if self.plot_pause == True:
            return

        t = np.linspace(0, 10, 101)
        # Shift the sinusoid as a function of time.
        self.signal1.set_data(t, np.sin(t + time.time()))
        self.signal2.set_data(t, np.sin(2 * t + time.time()))
        self.signal1.figure.canvas.draw()
        self.signal2.figure.canvas.draw()

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

        return

    def start_window(serial_ports, msg_list):
        # Check whether there is already a running QApplication (e.g., if running
        # from an IDE).
        qapp = QtWidgets.QApplication.instance()
        if not qapp:
            qapp = QtWidgets.QApplication(sys.argv)

        app = RTPlotWindow(serial_ports, msg_list)
        app.show()
        app.activateWindow()
        app.raise_()
        qapp.exec()
