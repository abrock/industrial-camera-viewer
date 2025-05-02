import QtQuick 2.12
import QtQuick.Window 2.15
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.12
import Qt.labs.settings 1.0

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("Camera-Viewer")

    TabView {
        width: parent.width
        height: 20

        Tab {
            title: "Acquisition"
            anchors.topMargin: 9
            anchors.leftMargin: 9

            RowLayout {
                Settings {
                    category: "camera_viewer"
                    id: set
                    property int exposure: 10000
                    property int ref_a: 0
                    property int ref_b: 100
                    property int test_val : 50
                    property int average_count: 3
                }
                ColumnLayout {
                    anchors.topMargin: 9
                    anchors.leftMargin: 9
                    RowLayout {
                        Text {
                            text: qsTr("Exposure time [us]")
                        }
                        SpinBox {
                            value: set.exposure
                            stepSize: 1
                            maximumValue: 300000
                            id: exposure
                            onValueChanged: {
                                set.exposure = value
                                cameraManager.setExposure(value)
                            }
                            Component.onCompleted: cameraManager.setExposure(value)
                            Connections {
                                target: cameraManager
                                function onRequestedExposure(val) {
                                    exposure.value = val;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
