#include <QDebug>

#include "movehistorymodel.h"

MoveHistoryModel::MoveHistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    _moves.clear();
    _playerToMove = Piece::White;
}

/*virtual*/ int MoveHistoryModel::rowCount(const QModelIndex &parent) const /*override*/
{
    Q_ASSERT(!parent.isValid());
    return _moves.count();
}

/*virtual*/ int MoveHistoryModel::columnCount(const QModelIndex &parent) const /*override*/
{
    Q_ASSERT(!parent.isValid());
    return 2;
}

/*virtual*/ QVariant MoveHistoryModel::data(const QModelIndex &index, int role) const /*override*/
{
    if (!index.isValid())
        return QVariant();
    Q_ASSERT(index.column() < 2);
    Q_ASSERT(index.row() < _moves.count());

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return _moves.at(index.row()).at(index.column()).move;
    }
    return QVariant();
}

/*virtual*/ bool MoveHistoryModel::setData(const QModelIndex &index, const QVariant &value, int role) /*override*/
{
    if (!index.isValid())
        return false;
    Q_ASSERT(index.column() < 2);
    Q_ASSERT(index.row() < _moves.count());

    if (role == Qt::EditRole)
    {
        _moves[index.row()][index.column()].move = value.toString();
        emit dataChanged(index, index, { role });
        return true;
    }
    return false;
}

/*virtual*/ bool MoveHistoryModel::insertRows(int row, int count, const QModelIndex &parent) /*override*/
{
    Q_ASSERT(!parent.isValid());
    if (count < 1)
        return false;
    // can only append new rows at the end (not in the middle)
    if (row != _moves.count())
        return false;

    // resize `_moves` to have `count` new rows at the end
    beginInsertRows(parent, row, row + count - 1);
    _moves.resize(_moves.count() + count);
    endInsertRows();
    return true;
}

/*virtual*/ bool MoveHistoryModel::removeRows(int row, int count, const QModelIndex &parent) /*override*/
{
    Q_ASSERT(!parent.isValid());
    if (count < 1)
        return false;
    // can only remove rows at the end (not in the middle)
    if (row != _moves.count() - 1)
        return false;

    // resize `_moves` to have `count` fewer rows at the end
    beginRemoveRows(parent, row, row + count - 1);
    _moves.resize(_moves.count() - count);
    endRemoveRows();
    return true;
}

/*virtual*/ void MoveHistoryModel::clear()
{
    beginResetModel();
    _moves.clear();
    _playerToMove = Piece::White;
    endResetModel();
    // always show new (blank) row for white's next move
    insertRow(0);
}

const QString &MoveHistoryModel::textOfMove(int turn, Piece::PieceColour player) const
{
    // return the text of the move for turn and player
    Q_ASSERT(turn >= 0 && turn < _moves.count());
    return _moves.at(turn).at(player).move;
}

const QString MoveHistoryModel::textOfLastMoveMade() const
{
    // return the text of the last move made
    int turn = rowCount() - 1;
    Piece::PieceColour player = Piece::opposingColour(_playerToMove);
    if (player == Piece::Black)
        if (--turn < 0)
            return QString();
    return textOfMove(turn, player);
}

void MoveHistoryModel::appendMove(Piece::PieceColour player, const QString &text)
{
    // append the latest move by player to the move history
    // note that we only allow appending of latest move, no kind of inserting/replacing
    Q_ASSERT(player == _playerToMove);
    // we always show new (blank) row for white's next move
    Q_ASSERT(rowCount() > 0);

    // set the text of the move in the (last row of) the model
    QModelIndex index(createIndex(rowCount() - 1, _playerToMove));
    setData(index, text);
    // switch which player is to move next
    _playerToMove = Piece::opposingColour(_playerToMove);
    // if it's a move by white append a new (blank) row
    if (_playerToMove == Piece::White)
        insertRow(_moves.count());

    // let outside world a move has been appended
    emit moveAppended();
}

void MoveHistoryModel::removeLastMove()
{
    // remove the latest move by player from the move history
    // (used when undoing moves)
    // note that we only allow removing of latest move, no kind of removing/replacing previous moves
    Q_ASSERT(rowCount() > 0);

    // if awaiting a move by white remove the last row (which contains the next white move)
    if (_playerToMove == Piece::White)
        removeRow(rowCount() - 1);
    // switch which player is to move next
    _playerToMove = Piece::opposingColour(_playerToMove);
    // clear the text of the move in the (last row of) the model
    Q_ASSERT(rowCount() > 0);
    QModelIndex index(createIndex(rowCount() - 1, _playerToMove));
    setData(index, QVariant());

    // let outside world a move has been appended
    emit lastMoveRemoved();
}

void MoveHistoryModel::saveMoveHistory(QTextStream &ts, bool insertTurnNumber /*= true*/) const
{
    // save the text of moves from model to file
    int lastMove = rowCount() - 1;
    // don't output the last, blank row
    if (lastMove >= 0 && textOfMove(lastMove, Piece::White).isEmpty())
        lastMove--;
    for (int i = 0; i <= lastMove; i++)
    {
        if (insertTurnNumber)
            ts << i + 1 << ". ";
        ts << textOfMove(i, Piece::White) << '\t' << textOfMove(i, Piece::Black) << Qt::endl;
    }
}

