import QtQuick 1.1

Item {
    id: actionRoot

    property bool isHighlight
    property bool isStrongHighlight

    property bool isCurrent

    property string actionIconURL
    property bool hoverScalesIcons: true

    property bool transparentStyle: false

    property alias isHovered: actionMouseArea.containsMouse

    signal triggered()
    signal hovered()
    signal hoverLeft()
    signal held()

    Rectangle {
        id: actionRect

        radius: width / 10
        smooth: true

        anchors.fill: parent
        anchors.margins: hoverScalesIcons ? 2 : 0
        border.width: transparentStyle ? 0 : (isStrongHighlight ? 2 : 1)
        border.color: actionRoot.isHighlight ? colorProxy.color_ToolButton_SelectedBorderColor : colorProxy.color_ToolButton_BorderColor

        gradient: Gradient {
            GradientStop {
                position: 0
                color: transparentStyle ? "#00000000" : (actionRoot.isHighlight ? colorProxy.color_ToolButton_SelectedTopColor : colorProxy.color_ToolButton_TopColor)
            }
            GradientStop {
                position: 1
                color: transparentStyle ? "#00000000" : (actionRoot.isHighlight ? colorProxy.color_ToolButton_SelectedBottomColor : colorProxy.color_ToolButton_BottomColor)
            }
        }

        states: [
            State {
                name: "current"
                when: actionRoot.isCurrent && !actionMouseArea.containsMouse
                PropertyChanges { target: actionRect; anchors.margins: 0 }
            },
            State {
                name: "hovered"
                when: actionMouseArea.containsMouse && !actionMouseArea.pressed
                PropertyChanges { target: actionRect; border.color: "white"; anchors.margins: 0 }
            },
            State {
                name: "pressed"
                when: actionMouseArea.containsMouse && actionMouseArea.pressed
                PropertyChanges { target: actionRect; border.color: "black" }
            }
        ]

        transitions: [
            Transition {
                from: ""
                to: "hovered,current"
                reversible: true
                PropertyAnimation { properties: "border.color,anchors.margins"; duration: 200 }
            }
        ]

        Image {
            id: actionImageElem

            anchors.fill: parent

            source: actionIconURL + '/' + width
            smooth: true
            cache: false
        }

        MouseArea {
            id: actionMouseArea

            anchors.fill: parent
            hoverEnabled: true

            onClicked: actionRoot.triggered()
            onPressAndHold: actionRoot.held()
            onEntered: actionRoot.hovered()
            onExited: actionRoot.hoverLeft()
        }
    }
}
