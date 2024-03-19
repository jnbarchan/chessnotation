#include <QColor>
#include <QDebug>
#include <QDir>

#include "pieceimages.h"

PieceImages::PieceImages(const QString &dirPath)
{
    // the piece set name is the directory name
    _pieceSetName = QDir(dirPath).dirName();

    images[0].clear();
    images[1].clear();

//    //TEMPORARY
//    produceFilesFromCombinedFile(dirPath);

    for (int i = 0; i <= 1; i++)
    {
        // load the individual files as a `QImage` into `images[][].image`, allowing for potential future colour change
        Piece::PieceColour player = static_cast<Piece::PieceColour>(i);
        PieceImageMap &pimp(images[player]);
        QString filePath = dirPath + "/" + ((player == Piece::White) ? "white_" : "black_");
        pimp[Piece::Bishop].image.load(filePath + "bishop.png");
        pimp[Piece::King].image.load(filePath + "king.png");
        pimp[Piece::Knight].image.load(filePath + "knight.png");
        pimp[Piece::Pawn].image.load(filePath + "pawn.png");
        pimp[Piece::Queen].image.load(filePath + "queen.png");
        pimp[Piece::Rook].image.load(filePath + "rook.png");

        // populate the corresponding `images[][].pixmap`, for direct usage
        revertPiecesColour(player);
    }
}

PieceImages::~PieceImages()
{
    images[0].clear();
    images[1].clear();
}

const QPixmap PieceImages::piecePixmap(Piece::PieceColour colour, Piece::PieceName name) const
{
    Q_ASSERT(images[colour].contains(name));
    return images[colour].value(name).pixmap;
}

void PieceImages::revertPiecesColour(Piece::PieceColour player)
{
    // revert the colour of `player`'s pieces to that in the originally loaded image
    _playerPiecesColour[player] = QColor();
    for (auto &ip : images[player])
        ip.pixmap = QPixmap::fromImage(ip.image);
}

void PieceImages::changePiecesColour(Piece::PieceColour player, const QColor &newColour)
{
    // change the colour of `player`'s pieces to `newColour`
    _playerPiecesColour[player] = newColour;
    QColor newColour2(newColour);
    for (auto &ip : images[player])
    {
        QImage image(ip.image);
        for (int y = 0; y < image.height(); y++)
            for (int x = 0; x < image.width(); x++)
            {
                QRgb rgba(image.pixel(x, y));
                newColour2.setAlpha(qAlpha(rgba));
                if (player == Piece::Black && qRed(rgba) + qGreen(rgba) + qBlue(rgba) < 100)    // "darkish"
                    image.setPixelColor(x, y, newColour2);
                else if (player == Piece::White && qRed(rgba) + qGreen(rgba) + qBlue(rgba) > 255 * 3 - 100)    // "lightish"
                    image.setPixelColor(x, y, newColour2);
            }
        ip.pixmap = QPixmap::fromImage(image);
    }
}

PieceImages::ColourCountMap PieceImages::countColours(const QImage &image) const
{
    // I wrote this function while looking at changing colours
    ColourCountMap colourCount;
    for (int y = 0; y < image.height(); y++)
        for (int x = 0; x < image.width(); x++)
            colourCount[image.pixel(x, y)]++;
    for (const auto key : colourCount.keys())
        if (colourCount.value(key) > 100)
            qDebug() << QString::number(key, 16).toUpper() << QColor(key).name() << colourCount.value(key);
    return colourCount;
}

void PieceImages::produceFilesFromCombinedFile(const QString &dirPath)
{
    // I wrote this function to read a "combined file" holding all the black & white piece images
    // and output individual files for each of the pieces so they can be used

    const QString combinedFile(dirPath + "/" + "alternative_all_pieces.png");
    QPixmap pixmap;
    pixmap.load(combinedFile);
    if (pixmap.isNull())
    {
        qDebug() << "Failed to load combined pieces file:" << combinedFile;
        return;
    }
    qDebug() << pixmap.rect();    // QRect(0,0 1800x800), 2 rows 6 columns
    int xPerSquare = pixmap.size().width() / 6, yPerSquare = pixmap.size().height() / 2;    // 300, 400
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 6; col++)
        {
            QString name;
            switch (col)
            {
            case 0: name = "rook"; break;
            case 1: name = "bishop"; break;
            case 2: name = "queen"; break;
            case 3: name = "king"; break;
            case 4: name = "knight"; break;
            case 5: name = "pawn"; break;
            }

            int y = row * yPerSquare;
            int x = col * xPerSquare;
            QPixmap clip(pixmap.copy(x, y, xPerSquare, yPerSquare).scaledToWidth(75));
            QString filePath = dirPath + "/" + ((row == 0) ? "black_" : "white_");
            filePath += name + ".png";

            clip.save(filePath);
        }
}
