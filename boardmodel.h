#ifndef BOARDMODEL_H
#define BOARDMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QObject>
#include <QTextStream>
#include <QUndoStack>

#include "piece.h"
#include "movehistorymodel.h"

class MoveUndoCommand;

class BoardModel : public QObject
{
    Q_OBJECT

public:
    BoardModel(QObject *parent = nullptr);
    ~BoardModel();

private:
    Piece *boardPieces[8][8];
    MoveHistoryModel *_moveHistoryModel;
    QUndoStack undoMovesStack;
    bool modelBeingReset;

public:
    struct BoardSquare
    {
        int row, col;
        BoardSquare() {}
        BoardSquare(int row, int col) { this->row = row; this->col = col; }
    };
    struct BoardSquareFromTo
    {
        BoardSquare from, to;
        BoardSquareFromTo(const BoardSquare &from, const BoardSquare &to) { this->from = from; this->to = to; }
    };

    inline MoveHistoryModel *moveHistoryModel() { return _moveHistoryModel; }
    inline Piece *pieceAt(int row, int col) const { return boardPieces[row][col]; }
    inline Piece *pieceAt(const BoardSquare &square) const { return pieceAt(square.row, square.col); }
    QList<BoardSquare> findPieces(Piece::PieceColour colour, Piece::PieceName name) const;
    bool couldMoveFromTo(const Piece &piece, const BoardSquare &squareFrom, const BoardSquare &squareTo, bool capture, bool enpassant) const;
    bool couldMoveFromTo(const BoardSquare &squareFrom, const BoardSquare &squareTo, bool capture, bool enpassant = false) const;
    bool parseAndMakeMove(Piece::PieceColour player, QString text);
    QAction *createUndoMoveAction(QObject *parent);
    QAction *createRedoMoveAction(QObject *parent);
    void doUndoableMoveCommand(const MoveUndoCommand &command);
    void undoUndoableMoveCommand(const MoveUndoCommand &command);
    void undoStackSetClean();
    bool undoStackIsClean() const;
    void saveMoveHistory(QTextStream &ts, bool insertTurnNumber = true) const;

private:
    bool obstructedMoveFromTo(const BoardSquare &squareFrom, const BoardSquare &squareTo) const;
    void clearBoardPieces(bool dontDelete = false);
    void addPiece(int row, int col, Piece::PieceColour colour, Piece::PieceName name);
    void removePiece(int row, int col);
    void movePiece(int rowFrom, int colFrom, int rowTo, int colTo);
    void setupInitialPieces();

public slots:
    void newGame();

signals:
    void startedNewGame();
    void modelReset();
    void pieceAdded(int row, int col, const Piece *);
    void pieceRemoved(const Piece *);
    void pieceMoved(int row, int col, const Piece *);
    void lastMoveMade(const QString &moveText);
    void undoStackCleanChanged(bool clean);
    void parserMessage(const QString &msg);
};


class MoveParser : public QObject
{
    Q_OBJECT

public:
    MoveParser(const BoardModel *model, Piece::PieceColour player);

    enum ParsedMoveType { Add, Remove, Move };
    struct ParsedMove
    {
        ParsedMoveType moveType;
        BoardModel::BoardSquare from, to;
        Piece piece{Piece::White, Piece::Pawn};
    };

    bool parse(const QString &text, QList<ParsedMove> &moves) const;

private:
    const BoardModel *model;
    Piece::PieceColour player;
    enum SideQualifier { NoSide, KingSide, QueenSide };
    bool parsePieceName(QString text, Piece::PieceName &name) const;
    bool parsePieceNameAndSide(QString text, Piece::PieceName &name, SideQualifier &side) const;
    QList<int> columnsForPieceAndSide(Piece::PieceName name, SideQualifier side) const;
    bool parseCastlingMove(const QString &text, const QStringList &tokens, QList<ParsedMove> &moves) const;
    bool parseMoveToMove(const QString &text, const QString &lhs, const QString &rhs, QList<ParsedMove> &moves) const;
    bool parseCaptureMove(const QString &text, const QString &lhs, const QString &rhs, QList<ParsedMove> &moves) const;
    void appendMovesForPawnPromotion(const Piece &piece, Piece::PieceName promotePawnToPiece, const BoardModel::BoardSquare &squareTo, QList<ParsedMove> &moves) const;
    bool checkPawnPromotionLegality(const QString &text, Piece::PieceName promotePawnToPiece, const Piece &piece, const BoardModel::BoardSquare &squareTo) const;
    bool parsePawnPromotionQualifier(QString &rhs, Piece::PieceName &promotePawnToPiece) const;
    void parseCheckQualifier(QString &rhs, bool &check) const;
    bool parseFullPieceSpecifier(const QString &text, QString &preQualifier, Piece::PieceName &name, QString &postQualifier) const;
    bool parsePiecePreQualifier(const QString &qualifier, Piece::PieceName name, QList<BoardModel::BoardSquare> &squares) const;
    bool parsePiecePostQualifier(const QString &qualifier, QList<BoardModel::BoardSquare> &squares) const;
    bool parseSquareSpecifier(const QString &specifier, int &row, QList<int> &cols) const;
    bool parsePieceMoveFrom(QString rhs, QList<BoardModel::BoardSquare> &squaresFrom) const;
    bool parseMoveTo(QString rhs, QList<BoardModel::BoardSquare> &squaresTo) const;
    QList<BoardModel::BoardSquareFromTo> resolveSquaresFromTo(const QList<BoardModel::BoardSquare> &squaresFrom, const QList<BoardModel::BoardSquare> &squaresTo, bool capture, bool enpassant, bool check) const;
    bool parseCaptureAt(QString rhs, QList<BoardModel::BoardSquare> &squaresTo, bool &enpassant) const;

signals:
    void parserMessage(const QString &msg) const;
};


class MoveUndoCommand : public QUndoCommand
{
public:
    MoveUndoCommand(BoardModel *boardModel, Piece::PieceColour player, const QString &text, const QList<MoveParser::ParsedMove> &moves);

    virtual void redo() override;
    virtual void undo() override;

    Piece::PieceColour player() const { return _player; }
    const QString &moveText() const { return _moveText; }
    const QList<MoveParser::ParsedMove> &moves() const { return _moves; }

private:
    BoardModel *_boardModel;
    Piece::PieceColour _player;
    QString _moveText;
    QList<MoveParser::ParsedMove> _moves;
};

#endif // BOARDMODEL_H
