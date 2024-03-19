#ifndef BOARDSCENE_H
#define BOARDSCENE_H

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>

class QPropertyAnimation;

#include "boardmodel.h"
#include "pieceimages.h"

class BoardPiecePixmapItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(int flash READ flashLevel WRITE setFlashLevel)

public:
    const Piece *piece = nullptr;
    static constexpr int flashLevelMax = 10;
    int flashLevel() const { return _flashLevel; }
    void setFlashLevel(int level) { _flashLevel = level; setVisible(_flashLevel < flashLevelMax / 2); }

private:
    int _flashLevel = 0;
};

class BoardScene : public QGraphicsScene
{
    Q_OBJECT

public:
    BoardScene(BoardModel *boardModel, QObject *parent = nullptr);
    ~BoardScene();

    PieceImages *pieceImages() const { return _pieceImages; }
    void loadPieceImages(const QString dirPath);
    void changePiecesColour(Piece::PieceColour player, const QColor &newColour);
    void revertPiecesColour(Piece::PieceColour player);

public slots:
    void addPiece(int row, int col, const Piece *piece);
    void removePiece(const Piece *piece);
    void movePiece(int row, int col, const Piece *piece);
    void resetFromModel();

private:
    BoardModel *boardModel;
    PieceImages *_pieceImages;
    QPropertyAnimation *itemMoveAnimation, *itemFlashAnimation;
    bool doAnimation, suspendAnimation;
    void terminateAnimation(QPropertyAnimation *&itemAnimation);
    void terminateAllAnimations();
    void animateAddPiece(BoardPiecePixmapItem *item);
    void animateRemovePiece(BoardPiecePixmapItem *item);
    void animateMovePiece(BoardPiecePixmapItem *item, const QPointF &startPos, const QPointF &endPos);
    void redrawAllPieces();
    BoardPiecePixmapItem *findItemForPiece(const Piece *piece) const;
    void rowColToScenePos(int row, int col, int &x, int &y) const;
    void rowColToScenePosForPiece(const BoardPiecePixmapItem *item, int row, int col, int &x, int &y) const;

protected:
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;
};

#endif // BOARDSCENE_H
