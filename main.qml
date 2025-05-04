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
                    property int gain: 0
                    property int min0: 0
                    property int min1: 0
                    property int min2: 0
                    property int max0: 255
                    property int max1: 255
                    property int max2: 255
                    property bool crosshairs: false
                    property bool crosshair_window: false
                    property bool auto_wb: true
                    property bool denoise: true
                    property int denoise_scale: 5
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
                    RowLayout {
                        Text {
                            text: qsTr("Gain [dB]")
                        }
                        SpinBox {
                            value: set.gain
                            stepSize: 1
                            maximumValue: 50
                            id: gain
                            onValueChanged: {
                                set.gain = value
                                cameraManager.setGain(value)
                            }
                            Component.onCompleted: cameraManager.setGain(value)
                            Connections {
                                target: cameraManager
                                function onRequestedGain(val) {
                                    gain.value = val;
                                }
                            }
                        }
                    }
                    RowLayout {
                        Button {
                            text: "Crosshairs"
                            checkable: true
                            checked: set.crosshairs
                            onCheckedChanged: {
                                set.crosshairs = checked
                                cameraManager.setCrosshairs(checked)
                            }
                        }
                        Button {
                            text: "Crosshair window"
                            checkable: true
                            checked: set.crosshair_window
                            onCheckedChanged: {
                                set.crosshair_window = checked
                                cameraManager.setCrosshairWindow(checked)
                            }
                        }
                    }
                    RowLayout {
                        Button {
                            text: "Denoise"
                            checkable: true
                            checked: set.denoise
                            onCheckedChanged: {
                                set.denoise = checked
                                cameraManager.setDenoise(checked)
                            }
                        }
                        SpinBox {
                            value: set.denoise_scale
                            stepSize: 1
                            maximumValue: 255
                            id: denoise_scale
                            onValueChanged: {
                                set.denoise_scale = value
                                cameraManager.setDenoiseScale(value)
                            }
                            Component.onCompleted: cameraManager.setDenoiseScale(value)
                        }
                    }
                    RowLayout {
                        Button {
                            text: "Auto-WB"
                            checkable: true
                            checked: set.auto_wb
                            onCheckedChanged: {
                                set.auto_wb = checked
                                cameraManager.setAutoWB(checked)
                            }
                        }
                    }
                    RowLayout {
                        Text {
                            text: "WB min0 / max0"
                        }
                        SpinBox {
                            value: set.min0
                            stepSize: 1
                            maximumValue: 255
                            id: min0
                            onValueChanged: {
                                set.min0 = value
                                cameraManager.setWBmin0(value)
                            }
                            Component.onCompleted: cameraManager.setWBmin0(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmin0(val) {
                                    min0.value = val
                                }
                            }
                        }
                        SpinBox {
                            value: set.max0
                            stepSize: 1
                            maximumValue: 255
                            id: max0
                            onValueChanged: {
                                set.max0 = value
                                cameraManager.setWBmax0(value)
                            }
                            Component.onCompleted: cameraManager.setWBmax0(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmax0(val) {
                                    max0.value = val
                                }
                            }
                        }
                    }
                    RowLayout {
                        Text {
                            text: "WB min1 / max1"
                        }
                        SpinBox {
                            value: set.min0
                            stepSize: 1
                            maximumValue: 255
                            id: min1
                            onValueChanged: {
                                set.min1 = value
                                cameraManager.setWBmin1(value)
                            }
                            Component.onCompleted: cameraManager.setWBmin1(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmin0(val) {
                                    min1.value = val
                                }
                            }
                        }
                        SpinBox {
                            value: set.max1
                            stepSize: 1
                            maximumValue: 255
                            id: max1
                            onValueChanged: {
                                set.max1 = value
                                cameraManager.setWBmax1(value)
                            }
                            Component.onCompleted: cameraManager.setWBmax1(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmax1(val) {
                                    max1.value = val
                                }
                            }
                        }
                    }
                    RowLayout {
                        Text {
                            text: "WB min2 / max2"
                        }
                        SpinBox {
                            value: set.min0
                            stepSize: 1
                            maximumValue: 255
                            id: min2
                            onValueChanged: {
                                set.min2 = value
                                cameraManager.setWBmin2(value)
                            }
                            Component.onCompleted: cameraManager.setWBmin2(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmin2(val) {
                                    min2.value = val
                                }
                            }
                        }
                        SpinBox {
                            value: set.max2
                            stepSize: 1
                            maximumValue: 255
                            id: max2
                            onValueChanged: {
                                set.max2 = value
                                cameraManager.setWBmax2(value)
                            }
                            Component.onCompleted: cameraManager.setWBmax2(value)
                            Connections {
                                target: cameraManager
                                function onRequestedWBmax2(val) {
                                    max2.value = val
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
