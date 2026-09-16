import QtQuick

Item {
    id: root

    required property Flickable flickable
    property int smoothness: 70
    property real wheelStep: 120
    property real targetContentY: flickable ? flickable.contentY : 0
    property bool writingContentY: false

    width: flickable ? flickable.width : 0
    height: flickable ? flickable.height : 0
    x: flickable ? flickable.contentX : 0
    y: flickable ? flickable.contentY : 0
    z: 1000

    function minimumContentY() {
        return flickable.originY;
    }

    function maximumContentY() {
        return Math.max(root.minimumContentY(), flickable.originY + flickable.contentHeight - flickable.height);
    }

    function boundedContentY(value) {
        return Math.max(root.minimumContentY(), Math.min(value, root.maximumContentY()));
    }

    function setContentY(value) {
        const boundedValue = root.boundedContentY(value);
        root.writingContentY = true;
        flickable.contentY = boundedValue;
        root.writingContentY = false;
        if (boundedValue !== value) {
            root.targetContentY = boundedValue;
            settleTimer.stop();
        }
    }

    function stop() {
        settleTimer.stop();
        root.targetContentY = flickable.contentY;
    }

    function enqueueWheelStep(delta) {
        flickable.cancelFlick();
        const basePosition = settleTimer.running ? root.targetContentY : flickable.contentY;
        root.targetContentY = root.boundedContentY(basePosition + delta);
        if (root.smoothness <= 0) {
            root.setContentY(root.targetContentY);
            return;
        }
        settleTimer.start();
    }

    WheelHandler {
        target: null
        acceptedModifiers: Qt.NoModifier
        onWheel: function (wheel) {
            if (wheel.pixelDelta.y !== 0) {
                // Trackpads already provide fine-grained positions and OS momentum.
                root.stop();
                root.setContentY(root.flickable.contentY - wheel.pixelDelta.y);
                wheel.accepted = true;
                return;
            }
            if (wheel.angleDelta.y === 0)
                return;

            root.enqueueWheelStep(-(wheel.angleDelta.y / 120.0) * root.wheelStep);
            wheel.accepted = true;
        }
    }

    Timer {
        id: settleTimer
        interval: 16
        repeat: true
        onTriggered: {
            const distance = root.targetContentY - flickable.contentY;
            // Smoothness changes a constant velocity, never the acceleration profile.
            const pixelsPerSecond = 1600 - root.smoothness * 12;
            const frameDistance = pixelsPerSecond * interval / 1000;
            if (Math.abs(distance) <= frameDistance) {
                root.setContentY(root.targetContentY);
                settleTimer.stop();
                return;
            }
            root.setContentY(flickable.contentY + (distance > 0 ? frameDistance : -frameDistance));
        }
    }

    Connections {
        target: root.flickable
        function onContentYChanged() {
            if (!root.writingContentY) {
                settleTimer.stop();
                root.targetContentY = root.flickable.contentY;
            }
        }
        function onDraggingChanged() {
            if (root.flickable.dragging)
                root.stop();
        }
    }
}
