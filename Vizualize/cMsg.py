
import zmq
from PyQt5 import QtCore, QtWidgets
import pyqtgraph as pg
import numpy as np

class subcriber(QtCore.QObject):
    
    message = pg.QtCore.pyqtSignal(list)

    def __init__(self, packer = None):
        
        pg.QtCore.QObject.__init__(self)
        super().__init__(packer)
        #Socket to sub the local server
        context = zmq.Context()
        self.socket = context.socket(zmq.SUB)
        self.socket.connect("tcp://130.233.164.32:6660")
        self.socket.subscribe("")
        self.data = [None,None,None,None,None,None]
        #Listen only certain topics
        #for i in np.arange(0,2):
            #self.socket.setsockopt(zmq.SUBSCRIBE, i)
        #    self.socket.subscribe(i)

        self.running = True
        self.topic = None

    def loop(self):
        while self.running:
            #data = self.socket.recv()
            data = self.socket.recv_multipart()
            if len(data) > 1:
                self.topic = data[0].decode("cp855").split("\n")[0]
                data = data[1].decode("cp855").split("\n")[0].split(",")
                self.data[:len(data)] = data
                #data = str(data.decode("cp855"))
                #data = self.socket.recv()
                #data = data.decode("cp855").split(",")
                #print("converting data", self.topic, self.data)
                try:
                    #self.message.emit([self.topic, float(self.data[0]), float(self.data[1]), float(self.data[2]), float(self.data[3]), float(self.data[4]), float(self.data[5])])
                    self.message.emit([self.topic, data])
                except:
                    print("failed to emit Data")
                    continue
            else:
                print("incorrect data")

class QtWindow(pg.QtWidgets.QMainWindow):

    def __init__(self,parent = None):
        pg.QtWidgets.QMainWindow.__init__(self, parent)

        #Create window and set the layout
        self.win = pg.GraphicsView()
        self.layout = pg.GraphicsLayout()
        self.win.setCentralItem(self.layout)
        
        self.Bplot = self.layout.addPlot(0,0,title = "Magnetic Field")
        self.tracPlot = self.layout.addPlot(0,1,title = "Tracker")
        self.currentPlot = self.layout.addPlot(1,0,title = "Currents")
        
        #Modify and tune subplots
        #mg field
        self.Bplot.setMouseEnabled(x = False, y= False)
        self.Bplot.setLabel("left", r'B [mT]')
        self.Bplot.setLabel("bottom", "time [s]")
        self.Bplot.setYRange(-150,150, padding = 0)
        self.B = self.Bplot.plot(pen = "r")
        
        #Init data
        self.BBuffer = np.zeros(100)
        self.BCounter = np.zeros_like(self.BBuffer)

        #Tracker
        self.tracPlot.setMouseEnabled(x = False, y= False)
        self.tracPlot.setLabel("left", r'x [pixel]')
        self.tracPlot.setLabel("bottom", r"y [pixel]")
        self.tracPlot.setYRange(0,1544, padding = 0)
        self.tracPlot.setXRange(0,2064, padding = 0)
        self.trac = self.tracPlot.plot(pen = "b")
        
        self.xBuffer = np.zeros(100)
        self.yBuffer = np.zeros_like(self.xBuffer)

        #Current
        self.currentPlot.setMouseEnabled(x = False, y= False)
        self.currentPlot.setLabel("bottom", r'time [s]')
        self.currentPlot.setLabel("left", r"current [A]")
        self.currentPlot.setXRange(0,60, padding = 0)
        self.currentPlot.setYRange(-3,3, padding = 0)
        self.currentMeasured = self.currentPlot.plot(pen = "k")
        self.currentTarget = self.currentPlot.plot(pen = "c")
        
        self.MeasuredBuffer = np.zeros(10000)
        self.TargetBuffer = np.zeros_like(self.MeasuredBuffer)
        self.CCounter = np.zeros_like(self.MeasuredBuffer)

        self.thread = QtCore.QThread()
        self.zeromqListener = subcriber()
        self.zeromqListener.moveToThread(self.thread)

        self.thread.started.connect(self.zeromqListener.loop) 
        self.zeromqListener.message.connect(self.signalReceiver)       

        pg.QtCore.QTimer.singleShot(0, self.thread.start)
        self.win.show()

    def signalReceiver(self, message):
        #self.textEdit.append("%s\n" %message)
        if message[0] == "0":
            self.BBuffer = np.roll(self.BBuffer,-1)
            self.BBuffer[-1] = float(message[1][0])

            self.BCounter = np.roll(self.BCounter,-1)
            self.BCounter[-1] = float(message[1][1])
            self.B.setData(self.BCounter,self.BBuffer)
        
        elif message[0] == "1":
            self.xBuffer = np.roll(self.xBuffer,-1)
            self.xBuffer[-1] = float(message[1][0])
            self.yBuffer = np.roll(self.yBuffer,-1)
            self.yBuffer[-1] = float(message[1][1])
            self.trac.setData(self.xBuffer,self.yBuffer)
        
        elif message[0] == "2":
            self.MeasuredBuffer = np.roll(self.MeasuredBuffer,-1)
            self.MeasuredBuffer[-1] = -float(message[1][1])

            self.TargetBuffer = np.roll(self.TargetBuffer,-1)
            self.TargetBuffer[-1] = float(message[1][0])

            self.CCounter = np.roll(self.CCounter,-1)
            self.CCounter[-1] = float(message[1][3])

            self.currentMeasured.setData(self.CCounter,self.MeasuredBuffer)
            self.currentTarget.setData(self.CCounter,self.TargetBuffer)
        else:
            print("incorrect data format")

    def closeEvent(self,event):
        self.zeromqListener.running = False
        self.thread.quit()
        self.thread.wait()

    def _check(self,x):
        if x is not None:
            return True
        else:
            return False