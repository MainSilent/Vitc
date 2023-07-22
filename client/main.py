import numpy as np
import re, sys, socket
from time import sleep
from threading import Thread
from PyQt5.QtGui import QImage, QPixmap
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtWidgets import QApplication, QStackedWidget, QMainWindow, QMessageBox, QWidget, QLabel, QVBoxLayout, QLineEdit, QPushButton


IP = ''
FPS = 0
PORT = 54102
BUFFER_SIZE = 8160
FRAME_SIZE = 783360
START_FRAME_CODE = b'G^kGtPhoMR0&Xj2k0z7P7@^0iM*#AL*UgzfEab$Gjhk@nzNGHse3sKHPW6U6KPqdrADB5p8KaEn9$Lq#LMyuata8fatqOj6Gd'
END_FRAME_CODE = b'@aejW9QqBnsR07eaUHy&MF7bEY#d2sG&Q7e6$bw^XWohJyH1ri8bdOUTpxJy2nu@q8e9HiFwZl*wanNFFPKS&DABtVpQjbBH2hd'

WIDTH = 480
HEIGHT = 272


class ConnectionThread(QThread):
    framebuf = b''
    connectionEstablished = pyqtSignal()
    connectionUpdateFrame = pyqtSignal(bytes)
    connectionFailed = pyqtSignal(str)

    def __init__(self, IP, PORT):
        super().__init__()
        self.IP = IP
        self.PORT = PORT

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5)

        try:
            sock.connect((self.IP, self.PORT))

            sock.send("".encode()) # This is for the server to get client address
            data, addr = sock.recvfrom(BUFFER_SIZE)
            self.check_frame(data)
            
            self.connectionEstablished.emit()

            while True:
                data, addr = sock.recvfrom(BUFFER_SIZE)
                self.check_frame(data)
        except Exception as e:
            self.connectionFailed.emit(str(e))
        finally:
            sock.close()

    def check_frame(self, data):
        if data == START_FRAME_CODE:
            self.framebuf = b''
        elif data == END_FRAME_CODE:
            self.connectionUpdateFrame.emit(self.framebuf)
        else:
            self.framebuf += data


class FrameWidget(QWidget):
    def __init__(self):
        super().__init__()

        self.label = QLabel()
        self.label.setScaledContents(True)
        self.label.setAlignment(Qt.AlignCenter)

        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.label)
        self.setLayout(layout)

        self.set_frame(b'\x00\x00\x00' * FRAME_SIZE, True)

    def nv12_to_rgb(self, frame, width=WIDTH, height=HEIGHT):
        from cv2 import cvtColor, COLOR_YUV2RGB_NV12

        yuv_frame = np.frombuffer(frame, dtype=np.uint8)
        yuv_frame = yuv_frame.reshape((height * 3 // 2, width))
        rgb_frame = cvtColor(yuv_frame, COLOR_YUV2RGB_NV12)

        return rgb_frame

    def set_frame(self, frame, rgb=False):
        global FPS

        try:
            if not rgb:
                frame = bytearray(frame)
                frame = self.nv12_to_rgb(frame)
            image = QImage(frame, WIDTH, HEIGHT, QImage.Format_RGB888)
            pixmap = QPixmap.fromImage(image).scaled(self.size())
            self.label.setPixmap(pixmap)
            FPS += 1
        except:
            ...


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Vitc")
        self.setMinimumSize(960, 544)

        self.ip_box = QLineEdit()
        self.ip_box.setFixedWidth(200)
        self.ip_box.setPlaceholderText("IP")
        self.ip_box.setMaxLength(15)

        self.conn_btn = QPushButton("Connect")
        self.conn_btn.setFixedWidth(150)
        self.conn_btn.setFixedHeight(35)
        self.conn_btn.clicked.connect(self.connect)
        self.conn_btn.setStyleSheet("background-color: green; margin-left: 50px; margin-top: 10px;")

        layout = QVBoxLayout()
        layout.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.ip_box)
        layout.addWidget(self.conn_btn)

        self.main_widget = QWidget()
        self.main_widget.setLayout(layout) 
        self.frame_widget = FrameWidget()

        self.stack = QStackedWidget()
        self.stack.addWidget(self.main_widget)
        self.stack.addWidget(self.frame_widget)
        self.setCentralWidget(self.stack)

    def hide_main(self, d=False):
        self.stack.setCurrentIndex(1 if d else 0)

    def disable_main(self, d=True):
        self.conn_btn.setText("Wait..." if d else "Connect")
        self.conn_btn.setDisabled(d)
        self.ip_box.setDisabled(d)

    def connect(self):
        IP = self.ip_box.text()
        is_valid = bool(re.match(r'^(\d{1,3}\.){3}\d{1,3}$', IP))

        if not is_valid:
            QMessageBox.critical(self, "Error", "Invalid IP")
        else:
            self.disable_main(True)

            self.connection_thread = ConnectionThread(IP, PORT)
            self.connection_thread.connectionEstablished.connect(self.on_connection_established)
            self.connection_thread.connectionUpdateFrame.connect(self.frame_widget.set_frame)
            self.connection_thread.connectionFailed.connect(self.on_connection_failed)
            self.connection_thread.start()

    def on_connection_established(self):
        self.hide_main(True)

    def on_connection_failed(self, error_message):
        self.hide_main(False)
        self.disable_main(False)
        QMessageBox.critical(self, "Error", error_message)

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_F11:
            if self.isFullScreen():
                self.showNormal()
            else:
                self.showFullScreen()


if __name__ == '__main__':
    app = QApplication(sys.argv)

    # --------- Print FPS ---------
    def pfps():
        global FPS

        while True:
            print('FPS: ' + str(FPS))
            FPS = 0
            sleep(1)
    Thread(target=pfps).start()
    # --------- Print FPS ---------

    window = MainWindow()
    window.show()
    sys.exit(app.exec_())