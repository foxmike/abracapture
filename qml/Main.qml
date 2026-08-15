import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    title: appController.appName
    minimumWidth: 800
    minimumHeight: 600

    Text {
        anchors.centerIn: parent
        text: appController.appName + " " + appController.appVersion
    }
}
