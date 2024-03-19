#include <QColorDialog>
#include <QDebug>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QToolButton>

#include "boardscene.h"
#include "piecesetdialog.h"

PieceSetDialog::PieceSetDialog(BoardScene *boardScene, const QString &appRootPath, QWidget *parent /*= nullptr*/)
    : QDialog(parent)
{
    this->boardScene = boardScene;
    this->appRootPath = appRootPath;
    setupUi();

    connect(btnChoosePieceSet, &QToolButton::clicked, this, &PieceSetDialog::choosePieceSet);
    connect(btnChooseWhitePieceColour, &QToolButton::clicked, this, [this]() { choosePieceColour(Piece::White); } );
    connect(btnChooseBlackPieceColour, &QToolButton::clicked, this, [this]() { choosePieceColour(Piece::Black); } );
}

void PieceSetDialog::setupUi()
{
    this->setWindowTitle("Piece Set");
    QFormLayout *formLayout = new QFormLayout;
    this->setLayout(formLayout);

    // show/choose the current piece set
    this->lblPieceSetName = new QLabel(boardScene->pieceImages()->pieceSetName());
    this->btnChoosePieceSet = createDotDotDotButton();
    formLayout->addRow("Piece set:", createLabelAndDotDotDotButtonWidget(lblPieceSetName, btnChoosePieceSet));

    // alter the colour for white/black pieces
    this->lblWhitePieceColour = new QLabel(showPieceColourName(boardScene->pieceImages()->piecesColour(Piece::White)));
    this->btnChooseWhitePieceColour = createDotDotDotButton();
    formLayout->addRow("White piece colour:", createLabelAndDotDotDotButtonWidget(lblWhitePieceColour, btnChooseWhitePieceColour));
    this->lblBlackPieceColour = new QLabel(showPieceColourName(boardScene->pieceImages()->piecesColour(Piece::Black)));
    this->btnChooseBlackPieceColour = createDotDotDotButton();
    formLayout->addRow("Black piece colour:", createLabelAndDotDotDotButtonWidget(lblBlackPieceColour, btnChooseBlackPieceColour));
}

QWidget *PieceSetDialog::createLabelAndDotDotDotButtonWidget(QLabel *label, QToolButton *btn)
{
    // create a widget for a label followed by a "..." button
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    label->setFixedWidth(200);
    layout->addWidget(label);
    layout->addWidget(btn);
    widget->setLayout(layout);
    return widget;

}

QToolButton *PieceSetDialog::createDotDotDotButton()
{
    // create small "..." button
    QToolButton *btn = new QToolButton(this);
    btn->setText("...");
    btn->setMaximumHeight(18);
    return btn;
}

/*slot*/ void PieceSetDialog::choosePieceSet()
{
    // allow user to pick a directory which contains the piece images
    const QString rootPath = appRootPath + "/images";
    QString dirPath = QFileDialog::getExistingDirectory(btnChoosePieceSet, "Choose Piece Set Directory", rootPath);
    if (dirPath.isEmpty())
        return;

    // (try to) load the images from the directory
    boardScene->loadPieceImages(dirPath);
    // update the name shown for the piece set
    lblPieceSetName->setText(boardScene->pieceImages()->pieceSetName());
    // and the names shown for the piece sets' colours
    lblWhitePieceColour->setText(showPieceColourName(boardScene->pieceImages()->piecesColour(Piece::White)));
    lblBlackPieceColour->setText(showPieceColourName(boardScene->pieceImages()->piecesColour(Piece::Black)));
}

/*slot*/ void PieceSetDialog::choosePieceColour(Piece::PieceColour player)
{
    // allow the user to choose a colour for white/black pieces
    QColorDialog dlg;
    dlg.setCurrentColor(boardScene->pieceImages()->piecesColour(player));
    connect(&dlg, &QColorDialog::currentColorChanged,
            this, [this, player](const QColor &colour) { changePieceColour(player, colour); } );
    int result = dlg.exec();
    if (result == Accepted)
        changePieceColour(player, dlg.currentColor());
    else if (result == Rejected)
        changePieceColour(player, QColor());
}

void PieceSetDialog::changePieceColour(Piece::PieceColour player, const QColor &colour)
{
    // change player's piece colour
    if (colour.isValid())
        boardScene->changePiecesColour(player, colour);
    else
        boardScene->revertPiecesColour(player);
    // update the name shown for the player's piece colour
    QLabel *lbl((player == Piece::White) ? lblWhitePieceColour : lblBlackPieceColour);
    lbl->setText(showPieceColourName(boardScene->pieceImages()->piecesColour(player)));
}

QString PieceSetDialog::showPieceColourName(const QColor &colour) const
{
    // return the name to show for a player's piece colour
    return colour.isValid() ? colour.name() : "(Original)";
}
