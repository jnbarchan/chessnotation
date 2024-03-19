#ifndef MOVEHISTORYMODEL_H
#define MOVEHISTORYMODEL_H

#include <QAbstractTableModel>
#include <QTextStream>
#include <QVector>

#include "piece.h"

class MoveHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MoveHistoryModel(QObject *parent = nullptr);

    // Basic functionality:
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // Add/remove data:
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    virtual void clear();

    inline Piece::PieceColour playerToMove() const { return _playerToMove; }
    const QString &textOfMove(int turn, Piece::PieceColour player) const;
    const QString textOfLastMoveMade() const;
    void appendMove(Piece::PieceColour player, const QString &text);
    void removeLastMove();
    void saveMoveHistory(QTextStream &ts, bool insertTurnNumber = true) const;

private:
    struct Move
    {
        QString move;
    };
    struct MovePair
    {
        Move whiteMove, blackMove;

        const Move &at(int i) const { if (i == 0) return whiteMove; else if (i == 1) return blackMove; else throw std::out_of_range("at(): index out of range"); }
        Move &operator[](int i) { if (i == 0) return whiteMove; else if (i == 1) return blackMove; else throw std::out_of_range("[]: index out of range"); }
        const Move &operator[](int i) const { if (i == 0) return whiteMove; else if (i == 1) return blackMove; else throw std::out_of_range("[]: index out of range"); }
    };
    QVector<MovePair> _moves;
    Piece::PieceColour _playerToMove;

signals:
    void moveAppended();
    void lastMoveRemoved();
};

#endif // MOVEHISTORYMODEL_H
