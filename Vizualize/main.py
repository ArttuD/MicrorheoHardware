
import sys
import cMsg 
import pyqtgraph as pg


app = pg.QtWidgets.QApplication(sys.argv)
mw = cMsg.QtWindow()
#QtWidgets.QGuiApplication.processEvents()
sys.exit(app.exec_())