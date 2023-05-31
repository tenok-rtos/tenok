import sys
import time

import numpy as np

from matplotlib.backends.qt_compat import QtWidgets
from matplotlib.backends.backend_qtagg import (
    FigureCanvas, NavigationToolbar2QT as NavigationToolbar)
from matplotlib.figure import Figure
from PyQt5.QtWidgets import (
    QApplication, QWidget, QComboBox, QHBoxLayout, QStyle)


class ApplicationWindow(QtWidgets.QMainWindow):
    def __init__(self):
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
        combo_ports.addItems(['/dev/ttyUSB0', '/dev/ttyUSB1'])
        combo_ports.setFixedSize(combo_ports.sizeHint())
        hbox_topbar.addWidget(combo_ports)

        btn_connect = QtWidgets.QPushButton(self._main)
        btn_connect.setText('Connect')
        btn_connect.setFixedSize(btn_connect.sizeHint())
        hbox_topbar.addWidget(btn_connect)

        checkbox_csv = QtWidgets.QCheckBox(self._main)
        checkbox_csv.setText('Save CSV')
        # checkbox_csv.setFixedSize(checkbox_csv.sizeHint())
        hbox_topbar.addWidget(checkbox_csv)

        combo_msgs = QComboBox(self._main)
        combo_msgs.addItems(['---message---'])
        combo_msgs.setFixedSize(combo_msgs.sizeHint())
        hbox_topbar.addWidget(combo_msgs)

        btn_pause = QtWidgets.QPushButton(self._main)
        btn_pause.setFixedSize(btn_pause.sizeHint())
        pixmapi = getattr(QStyle, 'SP_MediaPause')
        icon = self.style().standardIcon(pixmapi)
        btn_pause.setIcon(icon)
        hbox_topbar.addWidget(btn_pause)

        layout.addLayout(hbox_topbar)

        #======#
        # Plot #
        #======#
        static_canvas = FigureCanvas(Figure(figsize=(5, 3)))
        layout.addWidget(static_canvas)
        layout.addWidget(NavigationToolbar(static_canvas, self))

        dynamic_canvas = FigureCanvas(Figure(figsize=(5, 3)))
        layout.addWidget(dynamic_canvas)
        layout.addWidget(NavigationToolbar(dynamic_canvas, self))

        self._static_ax = static_canvas.figure.subplots()
        self._static_ax.grid(color="lightGray")
        # static_canvas.figure.set_facecolor("lightGray")
        #
        t = np.linspace(0, 10, 501)
        self._static_ax.plot(t, np.tan(t), ".")

        self._dynamic_ax = dynamic_canvas.figure.subplots()
        # dynamic_canvas.figure.set_facecolor("lightGray")
        self._dynamic_ax.grid(color="lightGray")
        #
        t = np.linspace(0, 10, 101)
        # Set up a Line2D.
        self._line, = self._dynamic_ax.plot(t, np.sin(t + time.time()))
        self._timer = dynamic_canvas.new_timer(50)
        self._timer.add_callback(self._update_canvas)
        self._timer.start()

    def _update_canvas(self):
        t = np.linspace(0, 10, 101)
        # Shift the sinusoid as a function of time.
        self._line.set_data(t, np.sin(t + time.time()))
        self._line.figure.canvas.draw()


if __name__ == "__main__":
    # Check whether there is already a running QApplication (e.g., if running
    # from an IDE).
    qapp = QtWidgets.QApplication.instance()
    if not qapp:
        qapp = QtWidgets.QApplication(sys.argv)

    app = ApplicationWindow()
    app.show()
    app.activateWindow()
    app.raise_()
    qapp.exec()
