#include "FBController.h"
#include "fbmanagement.h"
#include "log.h"

void FBController::setFramebufferID(int fbId){
    if(framebufferID != fbId){
        QDEBUG << "Re-register framebuffer " << framebufferID << "as" << fbId;
        qtfb::management::unregisterController(framebufferID);
    }
    framebufferID = fbId;
    qtfb::management::registerController(fbId, QPointer(this));
}

bool FBController::active() const {
    return _active;
}

void FBController::setActive(bool active){
    _active = active;
    emit activeChanged();
    emit framebufferSizeChanged();
    markedUpdate();
}

void FBController::setAllowScaling(bool allowScaling) {
    this->allowScaling = allowScaling;
    emit framebufferSizeChanged();
    markedUpdate();
}

FBController::~FBController(){
    qtfb::management::unregisterController(framebufferID);
}

void FBController::paint(QPainter *painter) {
    isMidPaint = true;
    QDEBUG << "FB Repaint triggered for " << framebufferID << ". Status: " << _active;
    // Do we have an SHM associated?
    if(this->image && this->_active) {
        // Cool. Paint it.
        if(allowScaling && fillMode != Pad) {
            painter->drawImage(this->convertQTFBRectToScreen(QRect(image->rect())), *image, image->rect());
        } else {
            painter->drawImage((width() - image->width()) / 2, (height() - image->height()) / 2, *image);
        }
    } else {
        /*
        QDEBUG << "Placeholder";
        QFont font = painter->font();
        font.setPointSize(50);
        font.setBold(true);
        painter->setFont(font);
        QRect rect(0, 0, width(), height());
        painter->fillRect(rect, QColor(255, 255, 0));
        painter->drawText(rect, "Unbound Framebuffer " + QString::number(framebufferID), Qt::AlignCenter | Qt::AlignTop);
        */
    }
    isMidPaint = false;
}

void FBController::associateSHM(QImage *image) {
    this->image = image;
    int key = framebufferID;
    QMetaObject::invokeMethod(this, [this, key]() {
        if(!qtfb::management::isControllerAssociated(key)) {
            // The framebuffer connection was terminated as we were
            // waiting for the event loop to process this request.
            this->image = nullptr;
            this->setActive(false);
            return;
        }
        this->setActive(this->image != nullptr);
    }, Qt::QueuedConnection);
}

void FBController::markedUpdate(const QRect &rect) {
    isMidPaint = true;

    if(!rect.isValid()) {
        // invalid rect means complete update
        update(rect);
        return;
    }

    if(image) {
        auto updateRect = convertQTFBRectToScreen(rect).adjusted(0, 0, 1, 1);
        update(updateRect);
    } else {
        update(rect);
    }
}

QPoint FBController::convertPointToQTFBPixels(const QPointF &input) {
    if(allowScaling && image) {
        int fbWidth = image->width();
        int fbHeight = image->height();

        if(fillMode == Stretch) {
            return QPoint(
                (input.x() * fbWidth ) / this->width(),
                (input.y() * fbHeight) / this->height()
            );
        }
        else if(fillMode == Pad) {
            return QPoint(
                input.x() - (width()  - fbWidth ) / 2,
                input.y() - (height() - fbHeight) / 2
            );
        }

        float fbAspectRatio = (float)fbWidth / fbHeight;
        bool  widthOrHeight = fbAspectRatio > (float)width() / height();

        if(   (fillMode == PreserveAspectFit  &&  widthOrHeight)
           || (fillMode == PreserveAspectCrop && !widthOrHeight)) {
            // scale to fill width, calculate height
            float calculatedHeight = width() / fbAspectRatio;
            return QPoint(
                (input.x() * fbWidth) / width(),
                ((input.y() - 0.5 * (height() - calculatedHeight)) * fbWidth) / width());
        } else {
            // scale to fill height, calculate width
            float calculatedWidth = height() * fbAspectRatio;
            return QPoint(
                ((input.x() - 0.5 * (width() - calculatedWidth)) * fbHeight) / height(),
                (input.y() * fbHeight) / height());
        }
    } else {
        return QPoint(input.x(), input.y());
    }
}

QRect FBController::convertQTFBRectToScreen(const QRect &input) {
    if(allowScaling && fillMode != Pad) {
        int fbWidth = image->width();
        int fbHeight = image->height();

        if(fillMode == Stretch) {
            return QRect(
                (input.left() * width()) / image->width(),
                (input.top() * height()) / image->height(),
                (input.width() * width()) / image->width(),
                (input.height() * height()) / image->height()
            );
        }

        float fbAspectRatio = (float)fbWidth / fbHeight;
        bool  widthOrHeight = fbAspectRatio > (float)width() / height();

        if(   (fillMode == PreserveAspectFit  &&  widthOrHeight)
           || (fillMode == PreserveAspectCrop && !widthOrHeight)) {
            // scale to fill width, calculate height
            float calculatedHeight = width() / fbAspectRatio;
            return QRect(
                (input.left()   * width()) / fbWidth,
                (input.top()    * width()) / fbWidth + (int)(0.5 * (height() - calculatedHeight)),
                (input.width()  * width() + fbWidth - 1) / fbWidth, // round width up
                (input.height() * width() + fbWidth - 1) / fbWidth  // round height up
            );
        } else {
            // scale to fill height, calculate width
            float calculatedWidth = height() * fbAspectRatio;
            return QRect(
                (input.left()   * height()) / fbHeight + (int)(0.5 * (width() - calculatedWidth)),
                (input.top()    * height()) / fbHeight,
                (input.width()  * height() + fbHeight - 1) / fbHeight, // round width up
                (input.height() * height() + fbHeight - 1) / fbHeight  // round height up
            );
        }
    } else {
        return input.translated((width() - image->width()) / 2, (height() - image->height()) / 2);
    }
}

void FBController::mouseEvent(QMouseEvent *me, int inputType) {
    bool reject = false;
    if(framebufferID != -1 && !me->points().isEmpty()) {
        const QEventPoint &point = me->points()[0];
        QPoint conv = convertPointToQTFBPixels(point.position());

        // only forward and accept events that fall into the framebuffer display region
        if(image && (conv.x() < 0 || conv.x() > image->width() || conv.y() < 0 || conv.y() > image->height())) {
            reject = true;
        } else {
            qtfb::UserInputContents packet {
                .inputType = inputType,
                .devId = 0, // TODO - differentiate between pen / eraser.
                .x = conv.x(),
                .y = conv.y(),
                .d = (int) (point.pressure() * 100.0),
            };
            qtfb::management::forwardUserInput(framebufferID, &packet);
        }
    }

    if(!reject) {
        me->accept();
    }
}

void FBController::mousePressEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_PRESS);
}

void FBController::mouseMoveEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_UPDATE);
}

void FBController::mouseReleaseEvent(QMouseEvent *me) {
    mouseEvent(me, INPUT_PEN_RELEASE);
}

static inline void sendKeyEvent(int key, int pkt, qtfb::FBKey framebufferID) {
    if(framebufferID != -1) {
        qtfb::UserInputContents packet {
            .inputType = pkt,
            .devId = 0,
            .x = key,
            .y = 0,
            .d = 0,
        };
        qtfb::management::forwardUserInput(framebufferID, &packet);
    }
}

void FBController::virtualKeyboardKeyDown(int key) {
    sendKeyEvent(key, INPUT_VKB_PRESS, framebufferID);
}

void FBController::virtualKeyboardKeyUp(int key) {
    sendKeyEvent(key, INPUT_VKB_RELEASE, framebufferID);
}

void FBController::specialKeyDown(int key) {
    sendKeyEvent(key, INPUT_BTN_PRESS, framebufferID);
}

void FBController::specialKeyUp(int key) {
    sendKeyEvent(key, INPUT_BTN_RELEASE, framebufferID);
}

void FBController::touchEvent(QTouchEvent *me) {
    if(framebufferID != -1) {
        int lenPoints = me->points().length();
        if(lenPoints == 5 && !refreshedScreenAlready) {
            emit requestFullRefresh();
            refreshedScreenAlready = true;
            QDEBUG << "QTFB Force Refresh";
        }
        for(const QEventPoint& point : me->points()) {
            QPoint conv = convertPointToQTFBPixels(point.position());
            QPoint pressConv = convertPointToQTFBPixels(point.pressPosition());
            qtfb::UserInputContents packet {
                .inputType = INPUT_TOUCH_PRESS,
                .devId = point.id(),
                .x = conv.x(),
                .y = conv.y(),
                .d = 0,
            };
            switch(point.state()) {
                case QEventPoint::State::Pressed:
                    packet.inputType = INPUT_TOUCH_PRESS;
                    if(point.position().y() < 100) checkingGestureDragDown = true;
                    break;
                case QEventPoint::State::Released:
                    packet.inputType = INPUT_TOUCH_RELEASE;
                    // handle the drag down gesture
                    if(point.position().y() > 100 && point.position().y() < 400 && checkingGestureDragDown) {
                        emit dragDown();
                    }
                    checkingGestureDragDown = false;
                    // handle the force-refresh gesture
                    if(lenPoints == 1) {
                        // Last point was released. Free the force-refresh flag.
                        refreshedScreenAlready = false;
                    }
                    break;
                case QEventPoint::State::Updated:
                    packet.inputType = INPUT_TOUCH_UPDATE;
                    break;
                default: break;
            }
            // only forward touch points to the client that started inside the framebuffer area
            if(image && pressConv.x() > 0 && pressConv.x() < image->width() && pressConv.y() > 0 && pressConv.y() < image->height()) {
                qtfb::management::forwardUserInput(framebufferID, &packet);
            }
        }
    }
    me->accept();
}

static inline int translateKey(int qtKey) {
    switch(qtKey) {
        case Qt::Key_Right: return INPUT_BTN_X_RIGHT;
        case Qt::Key_Left: return INPUT_BTN_X_LEFT;
        case Qt::Key_Home: return INPUT_BTN_X_HOME;
    }
    return -1;
}

void FBController::keyPressEvent(QKeyEvent *ke) {
    int k = translateKey(ke->key());
    if(k != -1)
        specialKeyDown(k);
}

void FBController::keyReleaseEvent(QKeyEvent *ke) {
    int k = translateKey(ke->key());
    if(k != -1)
        specialKeyUp(k);
}

int FBController::refreshMode() const { return _refreshMode; }
void FBController::setRefreshMode(int rm) {
    if(rm != _refreshMode) {
        _refreshMode = rm;
        emit refreshModeChanged();
    }
}

QSize FBController::framebufferSize() const {
    if(image == nullptr) {
        return QSize();
    }
    return image->size();
}
