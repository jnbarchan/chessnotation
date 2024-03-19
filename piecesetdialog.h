#ifndef PIECESETDIALOG_H
#define PIECESETDIALOG_H

#include <QDialog>

class QLabel;
class QToolButton;

#include "piece.h"

class BoardScene;

class PieceSetDialog : public QDialog
{
    Q_OBJECT

public:
    PieceSetDialog(BoardScene *boardScene, const QString &appRootPath, QWidget *parent = nullptr);

private slots:
    void choosePieceSet();
    void choosePieceColour(Piece::PieceColour player);

private:
    BoardScene *boardScene;
    QString appRootPath;
    QLabel *lblPieceSetName, *lblWhitePieceColour, *lblBlackPieceColour;
    QToolButton *btnChoosePieceSet, *btnChooseWhitePieceColour, *btnChooseBlackPieceColour;
    void setupUi();
    QToolButton *createDotDotDotButton();
    QWidget *createLabelAndDotDotDotButtonWidget(QLabel *label, QToolButton *btn);
    void changePieceColour(Piece::PieceColour player, const QColor &colour);
    QString showPieceColourName(const QColor &colour) const;
};

#endif // PIECESETDIALOG_H
