import QtQuick

Item {
    id: root

    enum Method {
        UFast,
        Fast,
        Animate,
        UI,
        Content
    }

    property int displayMethod: Content

    onDisplayMethodChanged: () => {
        console.log("Would change display mode to: " + displayMethod);
    }
}
