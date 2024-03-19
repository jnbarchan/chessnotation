#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QPropertyAnimation>
#include <QtMath>

#include "pieceimages.h"
#include "boardscene.h"

BoardScene::BoardScene(BoardModel *boardModel, QObject *parent) :
    QGraphicsScene(parent)
{
    // attach the BoardModel
    Q_ASSERT(boardModel);
    this->boardModel = boardModel;
    this->_pieceImages = nullptr;
    this->itemMoveAnimation = this->itemFlashAnimation = nullptr;
    this->doAnimation = true;
    this->suspendAnimation = false;

    // attach model signals
    connect(boardModel, &BoardModel::pieceAdded, this, &BoardScene::addPiece);
    connect(boardModel, &BoardModel::pieceRemoved, this, &BoardScene::removePiece);
    connect(boardModel, &BoardModel::pieceMoved, this, &BoardScene::movePiece);
    connect(boardModel, &BoardModel::modelReset, this, &BoardScene::resetFromModel);
}

BoardScene::~BoardScene()
{
    if (_pieceImages)
        delete _pieceImages;
}

void BoardScene::loadPieceImages(const QString dirPath)
{
    // physically load all the piece images into `piecesImages[]` array
    PieceImages *newPieceImages = new PieceImages(dirPath);
    // if we have existing piece images (not first time) and could not find images in the new piece set directory
    // do not wipe out existing piece images
    if (_pieceImages && !newPieceImages->foundImages())
        return;
    delete _pieceImages;
    _pieceImages = newPieceImages;
    redrawAllPieces();
}

void BoardScene::revertPiecesColour(Piece::PieceColour player)
{
    // revert the colour of pieces (to original state) for `player` in `piecesImages[]` array
    _pieceImages->revertPiecesColour(player);
    redrawAllPieces();
}

void BoardScene::changePiecesColour(Piece::PieceColour player, const QColor &newColour)
{
    // change the colour of pieces for `player` in `piecesImages[]` array
    _pieceImages->changePiecesColour(player, newColour);
    redrawAllPieces();
}

void BoardScene::terminateAnimation(QPropertyAnimation *&itemAnimation)
{
    // terminate an animation (which might be) in progress
    // note that this does so "abrubtly", it does not "finish" the animatuon so items do not reach their final state
    if (itemAnimation)
    {
        if (itemAnimation->state() != QAbstractAnimation::Stopped)
            itemAnimation->stop();
        itemAnimation->deleteLater();
        itemAnimation = nullptr;
    }
}

void BoardScene::terminateAllAnimations()
{
    // terminate all animations (which might be) in progress
    terminateAnimation(itemMoveAnimation);
    terminateAnimation(itemFlashAnimation);
}


void BoardScene::animateAddPiece(BoardPiecePixmapItem *item)
{
    if (!doAnimation || suspendAnimation)
    {
        // do nothing, piece already added and shown
        return;
    }

    // only one flash animation allowed at a time
    terminateAnimation(itemFlashAnimation);
    Q_ASSERT(itemFlashAnimation == nullptr);
    // set off an animated move calling `item->setPos()`
    itemFlashAnimation = new QPropertyAnimation(item, "flash", this);
    itemFlashAnimation->setStartValue(item->flashLevelMax);
    itemFlashAnimation->setEndValue(0);
    itemFlashAnimation->setDuration(1000);
    itemFlashAnimation->setLoopCount(3);
    connect(itemFlashAnimation, &QPropertyAnimation::stateChanged,
            this, [=](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        Q_UNUSED(oldState);
        // when animation finishes or is stopped, show the piece
        if (newState == QAbstractAnimation::Stopped)
            item->setFlashLevel(0);
    } );
    itemFlashAnimation->start();
}

void BoardScene::animateRemovePiece(BoardPiecePixmapItem *item)
{
    if (!doAnimation || suspendAnimation)
    {
        // remove from scene and delete
        removeItem(item);
        delete item;
        return;
    }

    // if there is a move animation on the item we are about to delete it must be terminated
    if (itemMoveAnimation && itemMoveAnimation->targetObject() == item)
        terminateAnimation(itemMoveAnimation);
    // only one flash animation allowed at a time
    terminateAnimation(itemFlashAnimation);
    Q_ASSERT(itemFlashAnimation == nullptr);
    // set off an animated move calling `item->setPos()`
    itemFlashAnimation = new QPropertyAnimation(item, "flash", this);
    itemFlashAnimation->setStartValue(item->flashLevelMax);
    itemFlashAnimation->setEndValue(0);
    itemFlashAnimation->setDuration(1000);
    itemFlashAnimation->setLoopCount(3);
    connect(itemFlashAnimation, &QPropertyAnimation::stateChanged,
            this, [=](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        Q_UNUSED(oldState);
        // when animation finishes or is stopped, remove from scene and delete
        if (newState == QAbstractAnimation::Stopped)
        {
            removeItem(item);
            delete item;
        }
    } );
    itemFlashAnimation->start();
}

void BoardScene::animateMovePiece(BoardPiecePixmapItem *item, const QPointF &startPos, const QPointF &endPos)
{
    if (!doAnimation || suspendAnimation)
    {
        // just set its end position
        item->setPos(endPos);
        return;
    }

    // only one move animation allowed at a time
    terminateAnimation(itemMoveAnimation);
    Q_ASSERT(itemMoveAnimation == nullptr);
    // set off an animated move calling `item->setPos()`
    itemMoveAnimation = new QPropertyAnimation(item, "pos", this);
    itemMoveAnimation->setStartValue(startPos);
    itemMoveAnimation->setEndValue(endPos);
    // calculate the duration based on the distance (longer moves take longer animation time)
    double xDist(endPos.x() - startPos.x()), yDist(endPos.y() - startPos.y());
    double distance = (xDist != 0 || yDist != 0) ? qSqrt(xDist * xDist + yDist * yDist) : 0;
    itemMoveAnimation->setDuration(distance / 400 * 1000 + 200);
    connect(itemMoveAnimation, &QPropertyAnimation::stateChanged,
            this, [=](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        Q_UNUSED(oldState);
        // when animation finishes or is stopped, move the piece to its destination
        if (newState == QAbstractAnimation::Stopped)
            item->setPos(endPos);
    } );
    itemMoveAnimation->start();
}

/*slot*/ void BoardScene::addPiece(int row, int col, const Piece *piece)
{
    Q_ASSERT(piece);
    Q_ASSERT(!findItemForPiece(piece));
    // get correct pixmap image
    const QPixmap &pixmap(_pieceImages->piecePixmap(piece->colour, piece->name));
    // create and add pixmap item to scene
    BoardPiecePixmapItem *item = new BoardPiecePixmapItem;
    item->setPixmap(pixmap);
    addItem(item);
    // associate item with piece passed in
    item->piece = piece;
    // place at scene position
    int x, y;
    rowColToScenePosForPiece(item, row, col, x, y);
    item->setPos(x, y);

    // animate flashing piece
    animateAddPiece(item);
}

/*slot*/ void BoardScene::removePiece(const Piece *piece)
{
    Q_ASSERT(piece);
    // find pixmap item on scene
    BoardPiecePixmapItem *item = findItemForPiece(piece);
    Q_ASSERT(item);

    // animate flashing piece, when finished remove from scene and delete
    animateRemovePiece(item);
}

/*slot*/ void BoardScene::movePiece(int row, int col, const Piece *piece)
{
    Q_ASSERT(piece);
    // find pixmap item on scene
    BoardPiecePixmapItem *item = findItemForPiece(piece);
    Q_ASSERT(item);
    // move to scene position
    int x, y;
    rowColToScenePosForPiece(item, row, col, x, y);

    // animate the move
    animateMovePiece(item, item->pos(), QPointF(x, y));
}

/*slot*/ void BoardScene::resetFromModel()
{
    // terminate any existing animation
    terminateAllAnimations();
    // suspend any animation while resetting board
    suspendAnimation = true;
    // clear all existing graphics items
    clear();
    // query model for all pieces, adding them
    const Piece *piece;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((piece = boardModel->pieceAt(row, col)))
                addPiece(row, col, piece);
    // restore animation
    suspendAnimation = false;
}

void BoardScene::redrawAllPieces()
{
    // redraw all pieces
    // called from `loadPieceImages()`, the piece images and sizes may have changed
    // query model for all pieces, adding them
    const Piece *piece;
    BoardPiecePixmapItem *item;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((piece = boardModel->pieceAt(row, col)))
                if ((item = findItemForPiece(piece)))
                {
                    // set its pixmap
                    const QPixmap &pixmap(_pieceImages->piecePixmap(piece->colour, piece->name));
                    item->setPixmap(pixmap);
                    // set its position
                    int x, y;
                    rowColToScenePosForPiece(item, row, col, x, y);
                    item->setPos(x, y);
                }
}

BoardPiecePixmapItem *BoardScene::findItemForPiece(const Piece *piece) const
{
    Q_ASSERT(piece);
    // search all scene items for pixmap item associated with piece passed in
    QList<QGraphicsItem *> items = this->items();
    for (QGraphicsItem *item : items)
    {
       BoardPiecePixmapItem *pixmapItem = qgraphicsitem_cast<BoardPiecePixmapItem *>(item);
       if (!pixmapItem)
           continue;
       if (pixmapItem->piece == piece)
           return pixmapItem;
    }
    return nullptr;
}

void BoardScene::rowColToScenePos(int row, int col, int &x, int &y) const
{
    // convert a logical board (row, col) to an actual (x, y) scene position
    // (x, y) are top-left of a square
    Q_ASSERT(row >= 0 && row < 8);
    Q_ASSERT(col >= 0 && col < 8);
    // scene coordinates have (0, 0) at top left and y increases downwards
    // we want row/column to be (0, 0) at bottom left and row increases upwards
    x = col * 100;
    y = (7 - row) * 100;
}

void BoardScene::rowColToScenePosForPiece(const BoardPiecePixmapItem *item, int row, int col, int &x, int &y) const
{
    rowColToScenePos(row, col, x, y);
    // (x, y) are top-left of a square, adjust to centre piece
    const QSize &size(item->pixmap().size());
    x += (100 - size.width()) / 2;
    y += (100 - size.height()) / 2;
}

/*virtual*/ void BoardScene::drawBackground(QPainter *painter, const QRectF &rect) /*override*/
{
    // call the base method
    QGraphicsScene::drawBackground(painter, rect);

    // clip to `rect`
    painter->save();
    painter->setClipRect(rect);

    // draw that part of the board which lies in `rect`
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
        {
            int x, y;
            rowColToScenePos(row, col, x, y);
            QRectF r(x, y, 100, 100);
            bool white = ((row + col) & 1);
            if (r.intersects(rect))
                painter->fillRect(r, QBrush(white ? Qt::white :  Qt::gray, Qt::SolidPattern));
        }

    // draw a frame
    painter->drawRect(sceneRect());
    painter->restore();
}
