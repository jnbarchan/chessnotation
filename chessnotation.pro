QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    boardmodel.cpp \
    boardscene.cpp \
    boardview.cpp \
    main.cpp \
    mainwindow.cpp \
    movehistorymodel.cpp \
    piece.cpp \
    pieceimages.cpp \
    piecesetdialog.cpp

HEADERS += \
    boardmodel.h \
    boardscene.h \
    boardview.h \
    mainwindow.h \
    movehistorymodel.h \
    piece.h \
    pieceimages.h \
    piecesetdialog.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    images/piece_set_0/all_pieces.png \
    images/piece_set_0/black_bishop.png \
    images/piece_set_0/black_king.png \
    images/piece_set_0/black_knight.png \
    images/piece_set_0/black_pawn.png \
    images/piece_set_0/black_queen.png \
    images/piece_set_0/black_rook.png \
    images/piece_set_0/white_bishop.png \
    images/piece_set_0/white_king.png \
    images/piece_set_0/white_knight.png \
    images/piece_set_0/white_pawn.png \
    images/piece_set_0/white_queen.png \
    images/piece_set_0/white_rook.png \
    samplegames/badgame \
    samplegames/checkgame \
    samplegames/chernev1 \
    samplegames/chernev2 \
    samplegames/chernev46 \
    samplegames/game1 \
    samplegames/game2
