#ifndef PIECEIMAGES_H
#define PIECEIMAGES_H

#include <QImage>
#include <QPixmap>
#include <QMap>
#include <QString>

#include "piece.h"


class PieceImages
{
public:
    PieceImages(const QString &dirPath);
    ~PieceImages();

    inline const QString &pieceSetName() const { return _pieceSetName; }
    const QPixmap piecePixmap(Piece::PieceColour colour, Piece::PieceName name) const;
    bool foundImages() const { return !piecePixmap(Piece::White, Piece::King).isNull(); }
    const QColor &piecesColour(Piece::PieceColour player) const { return _playerPiecesColour[player]; }
    void revertPiecesColour(Piece::PieceColour player);
    void changePiecesColour(Piece::PieceColour player, const QColor &newColour);

private:
    QString _pieceSetName;
    QColor _playerPiecesColour[2];
    struct ImageAndPixmap
    {
        QImage image;
        QPixmap pixmap;
    };
    typedef QMap<Piece::PieceName, ImageAndPixmap> PieceImageMap;
    PieceImageMap images[2];
    typedef QMap<QRgb, int> ColourCountMap;
    ColourCountMap countColours(const QImage &image) const;
    void produceFilesFromCombinedFile(const QString &dirPath);
};

#endif // PIECEIMAGES_H
