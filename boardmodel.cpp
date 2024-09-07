#include <QDebug>
#include <QRegularExpression>
#include <QString>

#include "boardmodel.h"

BoardModel::BoardModel(QObject *parent) :
    QObject(parent)
{
    _moveHistoryModel = new MoveHistoryModel(this);
    modelBeingReset = false;
    clearBoardPieces(true);

    connect(&undoMovesStack, &QUndoStack::indexChanged, this, &BoardModel::undoStackIndexChanged);
}

BoardModel::~BoardModel()
{
    clearBoardPieces();
}

QList<BoardModel::BoardSquare> BoardModel::findPieces(Piece::PieceColour colour, Piece::PieceName name) const
{
    // return a list of all the squares occupied by a piece of given type & colour
    QList<BoardSquare> squares;
    const Piece *piece;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((piece = boardPieces[row][col]))
                if (piece->colour == colour && piece->name == name)
                    squares.append({row, col});
    return squares;
}

bool BoardModel::obstructedMoveFromTo(const BoardSquare &squareFrom, const BoardSquare &squareTo) const
{
    // return whether a piece obstructs a move from `squareFrom` to `squareTo`
    // move must be either straight or diagonal line
    // no attempt is made to look at the piece or the move, outside world is expected to do that

    // set distances to (signed) number of squares in col/row direction
    int colDistance = squareTo.col - squareFrom.col, rowDistance = squareTo.row - squareFrom.row;
    // ensure it's either straight or diagonal
    Q_ASSERT(colDistance == 0 || rowDistance == 0 || qAbs(colDistance) == qAbs(rowDistance));
    int colDelta = (colDistance < 0) ? -1 : (colDistance > 0) ? 1 : 0;
    int rowDelta = (rowDistance < 0) ? -1 : (rowDistance > 0) ? 1 : 0;
    // ensure delta is not (0, 0), else we will get stuck!
    Q_ASSERT(colDelta != 0 || rowDelta != 0);
    // look at each square between `squareFrom` and `squareTo` (both exclusive) for any piece, which would block move
    BoardSquare square(squareFrom);
    square.col += colDelta;
    square.row += rowDelta;
    while (square.col != squareTo.col || square.row != squareTo.row)
    {
        if (pieceAt(square))
            return true;
        square.col += colDelta;
        square.row += rowDelta;
    }
    return false;
}

bool BoardModel::couldMoveFromTo(const Piece &piece, const BoardSquare &squareFrom, const BoardSquare &squareTo, bool capture, bool enpassant /*= false*/) const
{
    // return whether the piece specified, if it were in `squareFrom` (which it may or may not be), could move to `squareTo`
    // if `capture` it is a capture (possibly enpassant) move else it is a move move

    // set distances to (signed) number of squares in col/row direction, from player's point of view
    int colDistance = squareTo.col - squareFrom.col, rowDistance = squareTo.row - squareFrom.row;
    if (piece.isBlack())
        rowDistance = -rowDistance;
    // can't move to square it is presently on
    if (colDistance == 0 && rowDistance == 0)
        return false;
    Piece *squareToPiece = pieceAt(squareTo);
    if (capture)
    {
        // square must be occupied by opposing piece
        if (!squareToPiece || squareToPiece->colour == piece.colour)
            return false;
        // and if it's enpassant both pieces must be a pawn
        if (enpassant)
            if (piece.name != Piece::Pawn || squareToPiece->name != Piece::Pawn)
                return false;
    }
    else
    {
        // can't move to square occupied by either side
        if (squareToPiece)
            return false;
    }

    switch (piece.name)
    {
    case Piece::King: {
        // kings move one square in any direction
        // note that we do not allow the special 2-square move for castling here as that is handled specially elsewhere
        if (qAbs(colDistance) <= 1 && qAbs(rowDistance) <= 1)
            return true;
        return false;
    }
        break;

    case Piece::Queen: {
        // queens move like rooks or bishops
        if (colDistance == 0 || rowDistance == 0 || qAbs(colDistance) == qAbs(rowDistance))
            if (!obstructedMoveFromTo(squareFrom, squareTo))
                return true;
        return false;
    }
        break;

    case Piece::Rook: {
        // rooks move straight
        // note that we do not allow the special 2/3-square move for castling here as that is handled specially elsewhere
        if (colDistance == 0 || rowDistance == 0)
            if (!obstructedMoveFromTo(squareFrom, squareTo))
                return true;
        return false;
    }
        break;

    case Piece::Bishop: {
        // bishops move diagonally
        if (qAbs(colDistance) == qAbs(rowDistance))
            if (!obstructedMoveFromTo(squareFrom, squareTo))
                return true;
        return false;
    }
        break;

    case Piece::Knight: {
        // knights move like knights move :)
        if ((qAbs(colDistance) == 2 && qAbs(rowDistance) == 1) || (qAbs(colDistance) == 1 && qAbs(rowDistance) == 2))
            return true;
        return false;
    }
        break;

    case Piece::Pawn: {
        if (capture)
        {
            // pawns move diagonally forward 1 square
            // must be in adjacent column
            if (colDistance != -1 && colDistance != 1)
                return false;
            if (enpassant)
            {
                // check for special enpassant capture
                // here `squareTo` will be the square *currently* occupied by the opposing pawn
                // so this will actually look like a "sideways" move to that pawn's square
                // outside world will then have to deal with adjusting the final position of the capturing pawn
                Q_ASSERT(squareToPiece->name == Piece::Pawn);
                // must be "sideways"
                if (rowDistance != 0)
                    return false;
                // captured pawn must be on 4th rank
                if (squareTo.row != (squareToPiece->isWhite() ? 3 : 4))
                    return false;
                // the 2 squares behind the captured pawn must be empty, else this can't be enpassant
                if (pieceAt(squareToPiece->isWhite() ? 2 : 5, squareTo.col) || pieceAt(squareToPiece->isWhite() ? 1 : 6, squareTo.col))
                    return false;
                return true;
            }
            else
            {
                if (rowDistance == 1)
                    return true;
            }
        }
        else
        {
            // pawns move straight forward 1 or possibly 2 squares
            // must be in same column
            if (colDistance != 0)
                return false;
            // pawns can move 1 square forward, or 2 if they are on their starting row
            if (rowDistance == 1)
                return true;
            else if (rowDistance == 2)
            {
                if (squareFrom.row == (piece.isWhite() ? 1 : 6))
                    if (!obstructedMoveFromTo(squareFrom, squareTo))
                        return true;
            }
        }
        return false;
    }
        break;
    }
    return false;
}

bool BoardModel::couldMoveFromTo(const BoardSquare &squareFrom, const BoardSquare &squareTo, bool capture, bool enpassant /*= false*/) const
{
    // return whether the piece in `squareFrom` could move to `squareTo`
    // if `capture` it is a capture (possibly enpassant) move else it is a move move
    const Piece *piece = pieceAt(squareFrom);
    Q_ASSERT(piece);
    return couldMoveFromTo(*piece, squareFrom, squareTo, capture, enpassant);
}

bool BoardModel::checkForCheck(BoardModel::BoardSquare &from, BoardModel::BoardSquare &to) const
{
    // see whether the opposing King is in check from any of player's pieces
    // if so, set `from` & `to` to the piece giving check and the opposing King receiving check
    Piece::PieceColour player = _moveHistoryModel->playerToMove();
    QList<BoardModel::BoardSquare> squaresOpposingKing(findPieces(Piece::opposingColour(player), Piece::King));
    if (squaresOpposingKing.length() != 1)
        return false;
    BoardModel::BoardSquare squareTo(squaresOpposingKing.at(0));
    // go through all player's pieces seeing if any of them could capture opposing King
    const Piece *piece;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
        {
            BoardModel::BoardSquare squareFrom(row, col);
            if ((piece = pieceAt(squareFrom)) && piece->colour == player)
                if (couldMoveFromTo(squareFrom, squareTo, true, false))
                {
                    from = squareFrom;
                    to = squareTo;
                    return true;
                }
        }
    return false;
}

void BoardModel::clearBoardPieces(bool dontDelete /*= false*/)
{
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
        {
            if (!dontDelete)
                delete boardPieces[row][col];
            boardPieces[row][col] = nullptr;
        }
}

void BoardModel::addPiece(int row, int col, Piece::PieceColour colour, Piece::PieceName name, Piece::SideQualifier side /*= Piece::NoSide*/)
{
    Q_ASSERT(boardPieces[row][col] == nullptr);
    Piece *piece = new Piece(colour, name, side);
    boardPieces[row][col] = piece;
    if (!modelBeingReset)
        emit pieceAdded(row, col, piece);
}

void BoardModel::removePiece(int row, int col)
{
    Q_ASSERT(boardPieces[row][col] != nullptr);
    const Piece *piece = boardPieces[row][col];
    delete boardPieces[row][col];
    boardPieces[row][col] = nullptr;
    if (!modelBeingReset)
        emit pieceRemoved(piece);
}

void BoardModel::movePiece(int rowFrom, int colFrom, int rowTo, int colTo)
{
    Q_ASSERT(boardPieces[rowFrom][colFrom] != nullptr);
    Q_ASSERT(boardPieces[rowTo][colTo] == nullptr);
    Piece *piece = boardPieces[rowFrom][colFrom];
    boardPieces[rowFrom][colFrom] = nullptr;
    boardPieces[rowTo][colTo] = piece;
    if (!modelBeingReset)
        emit pieceMoved(rowTo, colTo, piece);
}

void BoardModel::checkForCheckAnimation()
{
    if (modelBeingReset)
        return;
    // see if currently "in check" for animation
    BoardModel::BoardSquare from, to;
    if (checkForCheck(from, to))
        emit showCheck(from.row, from.col, to.row, to.col);
}

void BoardModel::setupInitialPieces()
{
    // populate with the initial pieces at the start of a game

    modelBeingReset = true;
    clearBoardPieces();

    for (int player = 0; player <= 1; player++)
    {
        Piece::PieceColour colour(static_cast<Piece::PieceColour>(player));
        bool isWhite = (colour == Piece::White);
        int row = isWhite ? 0 : 7;
        addPiece(row, 0, colour, Piece::Rook, Piece::QueenSide);
        addPiece(row, 1, colour, Piece::Knight, Piece::QueenSide);
        addPiece(row, 2, colour, Piece::Bishop, Piece::QueenSide);
        addPiece(row, 3, colour, Piece::Queen);
        addPiece(row, 4, colour, Piece::King);
        addPiece(row, 5, colour, Piece::Bishop, Piece::KingSide);
        addPiece(row, 6, colour, Piece::Knight, Piece::KingSide);
        addPiece(row, 7, colour, Piece::Rook, Piece::KingSide);
        row = isWhite ? 1 : 6;
        for (int col = 0; col < 8; col++)
            addPiece(row, col, colour, Piece::Pawn);
    }

    modelBeingReset = false;
    emit modelReset();
}

void BoardModel::newGame()
{
    undoMovesStack.clear();
    _moveHistoryModel->clear();
    Q_ASSERT(_moveHistoryModel->playerToMove() == Piece::White);
    setupInitialPieces();
    // let outside world we have (set up and) started a new game
    emit startedNewGame();
}

bool BoardModel::parseAndMakeMove(Piece::PieceColour player, QString text)
{
    // parse the "Descriptive" notation in `text`
    // return true => successfully parsed and move made
    // return false => some kind of failure (could not parse or could not make move)

    // remove *all* whitespace
    text = text.simplified();
    text.replace(" ", "");
    if (text.isEmpty())
        return false;

    // parse the move
    MoveParser mp(this, player);
    connect(&mp, &MoveParser::parserMessage, this, &BoardModel::parserMessage);
    QList<MoveParser::ParsedMove> moves;
    if (!mp.parse(text, moves))
        return false;
    Q_ASSERT(!moves.isEmpty());

    // make the move(s) on the board model
    // we do this by creating an undoable `MoveUndoCommand` and pushing it to the undo stack
    // that causes `doUndoableMoveCommand()` to be called first time
    MoveUndoCommand *command = new MoveUndoCommand(this, player, text, moves);
    undoMovesStack.push(command);

    return true;
}

QAction *BoardModel::createUndoMoveAction(QObject *parent)
{
    // create the "Undo Last Move" action
    return undoMovesStack.createUndoAction(parent);
}

QAction *BoardModel::createRedoMoveAction(QObject *parent)
{
    // create the "Redo Last Move" action
    return undoMovesStack.createRedoAction(parent);
}

void BoardModel::doUndoableMoveCommand(const MoveUndoCommand &command)
{
    // do a MoveUndoCommand, either first time or after an undo

    // make the move(s) on the board model
    for (const auto &move : command.moves())
        switch (move.moveType)
        {
        case MoveParser::Add:
            // add the piece stored in `move.to` & `piece`
            addPiece(move.to.row, move.to.col, move.piece.colour, move.piece.name);
            break;

        case MoveParser::Remove:
            // remove the piece stored in `move.to` & `piece`
            removePiece(move.to.row, move.to.col);
            break;

        case MoveParser::Move:
            // move the piece from `move.from` to `move.to`
            movePiece(move.from.row, move.from.col, move.to.row, move.to.col);
            break;
        }
    // see if currently "in check" for animation
    checkForCheckAnimation();

    // append the move to the history
    _moveHistoryModel->appendMove(command.player(), command.moveText());
    // emit signal with text of last move made (so UI can update)
    emit lastMoveMade(command.moveText());
}

void BoardModel::undoUndoableMoveCommand(const MoveUndoCommand &command)
{
    // undo a MoveUndoCommand, after redo have previously been called

    // remove the last move from the history
    _moveHistoryModel->removeLastMove();
    // emit signal with text of last move made, i.e. previous move (so UI can update)
    emit lastMoveMade(_moveHistoryModel->textOfLastMoveMade());

    // make the *opposite* move(s) in reverse direction on the board model
    for (int i = command.moves().length() - 1; i >=0; i--)
    {
        const auto &move(command.moves().at(i));
        switch (move.moveType)
        {
        case MoveParser::Add:
            // remove the piece stored in `move.to` & `piece`
            removePiece(move.to.row, move.to.col);
            break;

        case MoveParser::Remove:
            // add the piece stored in `move.to` & `piece`
            addPiece(move.to.row, move.to.col, move.piece.colour, move.piece.name);
            break;

        case MoveParser::Move:
            // reverse by moving the piece from `move.to` to `move.from`
            movePiece(move.to.row, move.to.col, move.from.row, move.from.col);
            break;
        }
    }
    // see if currently "in check" for animation
    checkForCheckAnimation();
}

void BoardModel::undoStackSetClean()
{
    // set the undoStack to currently be "clean"
    // this is called from OpenedGameRunner::doStepOneMove() each time a new move is successfully read, parsed and made
    // so we can tell whether we have returned to exactly this state later on
    // which in turn tells us whether we can pick up where we got to in `OpenedGameRunner` stepping
    undoMovesStack.setClean();
}

bool BoardModel::undoStackIsClean() const
{
    // return whether the undoStack is currently "clean"
    // this tells us whether
    // (a) any moves have been undone and have not been redone; or
    // (b) some other move(s) have been "manually"
    // either way if unclean this means we cannot afford to continue stepping through `OpenedGameRunner`
    return undoMovesStack.isClean();
}

void BoardModel::undoStackRestoreToClean()
{
    // restore the undoStack to currently be "clean"
    // this repeatedly calls `undo()` or `redo()` until the stack reaches the clean state
    int cleanIndex = undoMovesStack.cleanIndex();
    if (cleanIndex >= 0)
        undoMovesStack.setIndex(cleanIndex);
}

bool BoardModel::undoStackCanRestoreToClean() const
{
    // return whether the undoStack can restore to a "clean" state, i.e. `undoStackRestoreToClean()` can be called
    return (undoMovesStack.cleanIndex() >= 0 && !undoMovesStack.isClean());
}

void BoardModel::saveMoveHistory(QTextStream &ts, bool insertTurnNumber /*= true*/) const
{
    // save the text of moves from `_moveHistoryModel` to file
    _moveHistoryModel->saveMoveHistory(ts, insertTurnNumber);
}



MoveParser::MoveParser(const BoardModel *model, Piece::PieceColour player)
{
    this->model = model;
    this->player = player;
}

bool MoveParser::parsePieceName(QString text, Piece::PieceName &name) const
{
    // parse a piece name, like "K"
    // return true => successfully parsed, and `name` filled in for piece
    // return false => failed to parse
    text = text.toUpper();
    if (text == "K")
        name = Piece::King;
    else if (text == "Q")
        name = Piece::Queen;
    else if (text == "B")
        name = Piece::Bishop;
    else if (text == "KT" || text == "N")
        name = Piece::Knight;
    else if (text == "R")
        name = Piece::Rook;
    else if (text == "P")
        name = Piece::Pawn;
    else
        return false;
    return true;
}

bool MoveParser::parsePieceNameAndSide(QString text, Piece::PieceName &name, Piece::SideQualifier &side) const
{
    // parse a piece name with optional side qualifier, like "K" or "KB"
    // return true => successfully parsed, and `name` filled in for piece and `side` for side qualifier
    // return false => failed to parse
    text = text.toUpper();

    side = Piece::NoSide;
    if (text.length() > 1 && text[1].isLetter())
    {
        // could be side qualifier like "KB"
        if (text[0] == 'Q')
        {
            side = Piece::QueenSide;
            text = text.remove(0, 1);
        }
        else if (text[0] == 'K' && text[1] != 'T')
        {
            side = Piece::KingSide;
            text = text.remove(0, 1);
        }
    }

    if (!parsePieceName(text, name))
        return false;

    // cannot have "KK" or "QK"
    if (side != Piece::NoSide)
        if (name == Piece::King || name == Piece::Queen)
            return false;
    return true;
}

QList<int> MoveParser::columnsForPieceAndSide(Piece::PieceName name, Piece::SideQualifier side) const
{
    // return the list of columns which a piece-and-side could refer to, like "R", "KR" or "QBP"
    QList<int> cols;
    if (name == Piece::King)
        cols << 4;
    else if (name == Piece::Queen)
        cols << 3;
    else if (name == Piece::Bishop)
    {
        if (side != Piece::QueenSide)
            cols << 5;
        if (side != Piece::KingSide)
            cols << 2;
    }
    else if (name == Piece::Knight)
    {
        if (side != Piece::QueenSide)
            cols << 6;
        if (side != Piece::KingSide)
            cols << 1;
    }
    else if (name == Piece::Rook)
    {
        if (side != Piece::QueenSide)
            cols << 7;
        if (side != Piece::KingSide)
            cols << 0;
    }
    return cols;
}

bool MoveParser::parse(const QString &text, QList<ParsedMove> &moves) const
{
    // parse the text of a move
    // return true => successfully parsed, `moves` filled with one or more moves to make
    // return false => could not be parsed, or "ambiguous" or "impossible"
    moves.clear();

    // try for a move with a `-` (hyphen), i.e. some kind of move
    QStringList tokens = text.split('-');

    if (tokens.length() > 0 && (tokens[0].toUpper() == "O" || tokens[0] == "0"))
    {
        // "O-O" or "O-O-O" castling move
        return parseCastlingMove(text, tokens, moves);
    }
    if (tokens.length() == 2)
    {
        // a move like "P-K4"
        return parseMoveToMove(text, tokens[0], tokens[1], moves);
    }
    // move has a `-`, but we failed to parse it, e.g. too many `-`s
    if (tokens.length() > 1)
    {
        emit parserMessage(QString("Unrecognised input for apparently move-type move: \"%1\"").arg(text));
        return false;
    }

    // try for a move with an `x`, i.e. some kind of cpature
    tokens = text.split("x", Qt::KeepEmptyParts, Qt::CaseInsensitive);
    if (tokens.length() == 2)
    {
        // a capture like "PxP"
        return parseCaptureMove(text, tokens[0], tokens[1], moves);
    }
    // move has a `x`, but we failed to parse it, e.g. too many `x`s
    if (tokens.length() > 1)
    {
        emit parserMessage(QString("Unrecognised input for apparently capture-type move: \"%1\"").arg(text));
        return false;
    }

    emit parserMessage(QString("Unrecognised input for move: \"%1\"").arg(text));
    return false;
}

bool MoveParser::parseCastlingMove(const QString &text, const QStringList &tokens, QList<ParsedMove> &moves) const
{
    // try for a "O-O" or "O-O-O" castling move
    // fill `moves` with list of `ParsedMoves` for unique move found

    if (tokens.length() > 3 || tokens.length() < 2 || (tokens[1].toUpper() != "O" && tokens[1] != "0"))
    {
        emit parserMessage(QString("Unrecognised castling-type move: \"%1\"").arg(text));
        return false;
    }
    bool kingSide = true;
    if (tokens.length() == 3)
    {
        if (tokens[2].toUpper() != "O" && tokens[2] != "0")
        {
            emit parserMessage(QString("Unrecognised castling-type move: \"%1\"").arg(text));
            return false;
        }
        kingSide = false;
    }

    // set up the proposed moves for king & rook
    bool isWhite = (player == Piece::White);
    int row = isWhite ? 0 : 7;
    BoardModel::BoardSquare kingFrom(row, 4), kingTo(row, kingSide ? 6 : 2);
    BoardModel::BoardSquare rookFrom(row, kingSide ? 7 : 0), rookTo(row, kingSide ? 5 : 3);

    // find the player's king & rook in the right places
    const Piece *king = model->pieceAt(kingFrom);
    if (!king || king->name != Piece::King || king->colour != player)
    {
        emit parserMessage(QString("King not on King's square for castling-type move"));
        return false;
    }
    const Piece *rook = model->pieceAt(rookFrom);
    if (!rook || rook->name != Piece::Rook || rook->colour != player)
    {
        emit parserMessage(QString("Rook not on Rook's square for castling-type move"));
        return false;
    }

    // check no other pieces in the way
    if (model->pieceAt(kingTo) || model->pieceAt(rookTo) ||
            (!kingSide && model->pieceAt(row, 1)))
    {
        emit parserMessage(QString("Intervening pieces for castling-type move"));
        return false;
    }

    // move the king and the rook
    moves.append({ Move, kingFrom, kingTo });
    moves.append({ Move, rookFrom, rookTo });

    return true;
}

bool MoveParser::parseMoveToMove(const QString &text, const QString &lhs, const QString &rhs, QList<ParsedMove> &moves) const
{
    // try for a move like "P-K4"
    // fill `moves` with list of `ParsedMoves` for unique move found

    // parse the piece and the possible source squares to move from on the lhs
    QList<BoardModel::BoardSquare> squaresFrom;
    if (!parsePieceMoveFrom(lhs, squaresFrom))
    {
        emit parserMessage(QString("Unrecognised piece to move: \"%1\"").arg(lhs));
        return false;
    }
    if (squaresFrom.isEmpty())
    {
        emit parserMessage(QString("Could not find piece to move: \"%1\"").arg(lhs));
        return false;
    }

    QString rhs2(rhs);
    // see if there is "check" at the end of the rhs
    bool check;
    parseCheckQualifier(rhs2, check);
    // see if there is a (pawn) promotion ("=Q") at the end of the rhs
    Piece::PieceName promotePawnToPiece = Piece::Pawn;
    if (!parsePawnPromotionQualifier(rhs2, promotePawnToPiece))
        return false;

    // parse the possible destination squares to move to on the rhs
    QList<BoardModel::BoardSquare> squaresTo;
    if (!parseMoveTo(rhs2, squaresTo))
    {
        emit parserMessage(QString("Unrecognised square to move to: \"%1\"").arg(rhs));
        return false;
    }
    if (squaresTo.isEmpty())
    {
        emit parserMessage(QString("Could not find square to move piece to: \"%1\"").arg(rhs));
        return false;
    }

    // resolve which square(s) it must be from/to from all possible froms/tos
    QList<BoardModel::BoardSquareFromTo> squaresFromTo;
    squaresFromTo = resolveSquaresFromTo(squaresFrom, squaresTo, false, false, check);

    // if not unique square from and to this is either "impossible" or "ambiguous" and we are stuck
    if (squaresFromTo.length() == 0)
    {
        emit parserMessage(QString("Could not find a piece which can move to square: \"%1\"").arg(text));
        return false;
    }
    else if (squaresFromTo.length() > 1)
    {
        emit parserMessage(QString("Found more than one piece/square which satisfies move: \"%1\"").arg(text));
        return false;
    }
    // found unique from/to move
    BoardModel::BoardSquare squareFrom(squaresFromTo[0].from), squareTo(squaresFromTo[0].to);

    Piece *piece = model->pieceAt(squareFrom);
    Q_ASSERT(piece && piece->colour == player);
    // not allowed for a move if destination is occupied
    if (model->pieceAt(squareTo))
    {
        emit parserMessage(QString("Square to move to is occupied: \"%1\"").arg(text));
        return false;
    }

    // deal with pawn promotion
    if (!checkPawnPromotionLegality(text, promotePawnToPiece, *piece, squareTo))
        return false;

    // append a simple move from-to
    moves.append({ Move, squareFrom, squareTo });
    // if pawn promotion append to change piece
    if (promotePawnToPiece != Piece::Pawn)
        appendMovesForPawnPromotion(*piece, promotePawnToPiece, squareTo, moves);

    return true;
}

bool MoveParser::parseCaptureMove(const QString &text, const QString &lhs, const QString &rhs, QList<ParsedMove> &moves) const
{
    // try for a capture like "PxP"
    // fill `moves` with list of `ParsedMoves` for unique move found

    // parse the piece and the possible source squares to move from on the lhs
    QList<BoardModel::BoardSquare> squaresFrom;
    if (!parsePieceMoveFrom(lhs, squaresFrom))
    {
        emit parserMessage(QString("Unrecognised piece to move: \"%1\"").arg(lhs));
        return false;
    }
    if (squaresFrom.isEmpty())
    {
        emit parserMessage(QString("Could not find piece to move: \"%1\"").arg(lhs));
        return false;
    }

    QString rhs2(rhs);
    // see if there is "check" at the end of the rhs
    bool check;
    parseCheckQualifier(rhs2, check);
    // see if there is a (pawn) promotion ("=Q") at the end of the rhs
    Piece::PieceName promotePawnToPiece = Piece::Pawn;
    if (!parsePawnPromotionQualifier(rhs2, promotePawnToPiece))
        return false;

    // parse the possible piece/square to capture on the rhs
    QList<BoardModel::BoardSquare> squaresTo;
    bool enpassant;
    if (!parseCaptureAt(rhs2, squaresTo, enpassant))
    {
        emit parserMessage(QString("Unrecognised piece to capture: \"%1\"").arg(rhs));
        return false;
    }
    if (squaresTo.isEmpty())
    {
        emit parserMessage(QString("Could not find piece to capture: \"%1\"").arg(rhs));
        return false;
    }

    // resolve which square(s) it must be from/to from all possible froms/tos
    QList<BoardModel::BoardSquareFromTo> squaresFromTo;
    squaresFromTo = resolveSquaresFromTo(squaresFrom, squaresTo, true, enpassant, check);

    // if not unique square from and to this is either "impossible" or "ambiguous" and we are stuck
    if (squaresFromTo.length() == 0)
    {
        emit parserMessage(QString("Could not find a piece move which can capture: \"%1\"").arg(text));
        return false;
    }
    else if (squaresFromTo.length() > 1)
    {
        emit parserMessage(QString("Found more than one piece/square which satisfies capture: \"%1\"").arg(text));
        return false;
    }
    // found unique from/to capture
    BoardModel::BoardSquare squareFrom(squaresFromTo[0].from), squareTo(squaresFromTo[0].to);

    // not allowed for a capture if destination is not occupied by opposing piece
    Piece *piece = model->pieceAt(squareFrom);
    Q_ASSERT(piece && piece->colour == player);
    Piece *opposingPiece = model->pieceAt(squareTo);
    if (!opposingPiece || opposingPiece->colour == player)
    {
        emit parserMessage(QString("Square to capture is not occupied by opposing piece: \"%1\"").arg(text));
        return false;
    }

    // deal with pawn promotion
    if (!checkPawnPromotionLegality(text, promotePawnToPiece, *piece, squareTo))
        return false;

    // append to remove captured piece
    moves.append({ Remove, BoardModel::BoardSquare(), squareTo, *opposingPiece });
    if (enpassant)
    {
        // adjust `squareTo`, which is where the captured pawn actually is,
        // forward 1 square, which is where the capturing pawns actually moves to
        Q_ASSERT(piece->name == Piece::Pawn && opposingPiece->name == Piece::Pawn);
        Q_ASSERT(squareFrom.row == (piece->isWhite() ? 4 : 3) && squareTo.row == squareFrom.row);
        Q_ASSERT(qAbs(squareTo.col - squareFrom.col) == 1);
        squareTo.row = piece->isWhite() ? 5 : 2;
    }
    // append a simple move from-to
    moves.append({ Move, squareFrom, squareTo });
    // if pawn promotion append to change piece
    if (promotePawnToPiece != Piece::Pawn)
        appendMovesForPawnPromotion(*piece, promotePawnToPiece, squareTo, moves);

    return true;
}

void MoveParser::appendMovesForPawnPromotion(const Piece &piece, Piece::PieceName promotePawnToPiece, const BoardModel::BoardSquare &squareTo, QList<ParsedMove> &moves) const
{
    // append moves to replace a pawn being promoted by a piece
    Q_ASSERT(piece.name == Piece::Pawn);
    Q_ASSERT(promotePawnToPiece != Piece::Pawn && promotePawnToPiece != Piece::King);
    Q_ASSERT(squareTo.row == (piece.isWhite() ? 7 : 0));
    Piece newPiece(piece.colour, promotePawnToPiece);
    moves.append({ Remove, BoardModel::BoardSquare(), squareTo, piece });
    moves.append({ Add, BoardModel::BoardSquare(), squareTo, newPiece });
}

bool MoveParser::checkPawnPromotionLegality(const QString &text, Piece::PieceName promotePawnToPiece, const Piece &piece, const BoardModel::BoardSquare &squareTo) const
{
    // check for pawn promotion legality
    bool on8thRank = (squareTo.row == (piece.isWhite() ? 7 : 0));
    if (promotePawnToPiece != Piece::Pawn)
    {
        // if piece is being promoted to piece check it's a pawn
        if (piece.name != Piece::Pawn)
        {
            emit parserMessage(QString("Piece to be promoted is not a pawn: \"%1\"").arg(text));
            return false;
        }
        // if pawn is being promoted to piece check it's on the 8th rank
        if (!on8thRank)
        {
            emit parserMessage(QString("Pawn to be promoted is not on 8th rank: \"%1\"").arg(text));
            return false;
        }
    }
    else
    {
        // if pawn is on 8th rank check it is being promoted to piece
        if (piece.name == Piece::Pawn && on8thRank)
        {
            emit parserMessage(QString("Pawn on 8th rank missing \"=...\" promotion specifier: \"%1\"").arg(text));
            return false;
        }
    }

    return true;
}

bool MoveParser::parsePawnPromotionQualifier(QString &rhs, Piece::PieceName &promotePawnToPiece) const
{
    // see if there is a (pawn) promotion ("=Q") at the end of the rhs
    // if there is, set `promotePawnToPiece` to the piece to promote to, else set it to `Piece::Pawn`
    // change `rhs` to have any promotion removed
    promotePawnToPiece = Piece::Pawn;
    QRegularExpressionMatch match;
    if (rhs.contains(QRegularExpression("^(.*)=(.*)$", QRegularExpression::CaseInsensitiveOption), &match))
    {
        QString promotion = match.captured(2);
        if (!parsePieceName(promotion, promotePawnToPiece))
        {
            emit parserMessage(QString("Could not parse piece to promote to: \"%1\"").arg(rhs));
            return false;
        }
        if (promotePawnToPiece == Piece::Pawn || promotePawnToPiece == Piece::King)
        {
            emit parserMessage(QString("Illegal piece to promote to: \"%1\"").arg(rhs));
            return false;
        }
        rhs = match.captured(1);
    }
    return true;
}

void MoveParser::parseCheckQualifier(QString &rhs, bool &check) const
{
    // see if there is a "check" ("ch" or "+") at the end of the rhs
    // set `check` correspondingly
    // change `rhs` to have any check removed
    check = false;
    QRegularExpressionMatch match;
    if (rhs.contains(QRegularExpression("^(.*)(ch\\.?|\\+)$", QRegularExpression::CaseInsensitiveOption), &match))
    {
        check = true;
        rhs = match.captured(1);
    }
}

bool MoveParser::parseFullPieceSpecifier(const QString &text, QString &preQualifier, Piece::PieceName &name, QString &postQualifier) const
{
    // parse a "full" piece specifier, like "K" or "QB" or "KKtP" or "R(B1)"
    // set `preQualifier` to anything coming before the piece, `name` to the (parsed) piece, `postQualifier` to anything coming after the piece
    preQualifier = postQualifier = "";
    QString pieceName;
    for (int i = 0; i < text.length(); i++)
    {
        QChar ch = text.at(i);
        if (ch == '(')
        {
            postQualifier = text.mid(i);
            break;
        }
        preQualifier += pieceName;
        pieceName = ch;
        if (ch.toUpper() == 'K' && i + 1 < text.length() && text.at(i + 1).toLower() == 't')
            pieceName += text.at(++i);
    }
    return parsePieceName(pieceName, name);
}

bool MoveParser::parsePiecePreQualifier(const QString &qualifier, Piece::PieceName name, QList<BoardModel::BoardSquare> &squares) const
{
    // parse a preceding "side-column" qualifier, like "K" or "QB"
    // `name` is the piece being qualified
    // `squares` is all the squares the piece could be on, reduce this to satisfy the qualifier
    Piece::PieceName columnName;
    Piece::SideQualifier side;
    if (!parsePieceNameAndSide(qualifier, columnName, side))
        return false;
    if (name == Piece::Pawn)
    {
        // if moving piece is a pawn we have to allow for "K" or "QB" or "B"
        // figure which columns it could apply to
        QList<int> cols = columnsForPieceAndSide(columnName, side);
        if (cols.length() == 0)
            return false;
        // only accept pawns currently located in those column(s)
        // there is a debate about whether "KP" should mean
        // (a) pawn which started on King's column, or
        // (b) pawn which is presently situated on King's column
        // we take the latter interpretation (actually for pawns this is probably the only correct one)
        for (int i = squares.length() - 1; i >= 0; i--)
            if (!cols.contains(squares[i].col))
                squares.removeAt(i);
    }
    else
    {
        // if piece is not a pawn only "K" or "Q" is allowed
        if (name == Piece::King || name == Piece::Queen)    // "K"/"Q" cannot have any qualifier
            return false;
        if (side != Piece::NoSide)    // "QB" not allowed for non-pawn
            return false;
        // set `side` from `columnName`
        if (columnName == Piece::King)
            side = Piece::KingSide;
        else if (columnName == Piece::Queen)
            side = Piece::QueenSide;
        else
            return false;
        // only accept pieces which started on the K/Q side
        // there is a debate about whether "KR" should mean
        // (a) rook which started on King's side, or
        // (b) rook which is presently situated on the King's side
        // we take the former interpretation
        Piece *piece;
        for (int i = squares.length() - 1; i >= 0; i--)
            if ((piece = model->pieceAt(squares[i])) != nullptr && piece->side != side)
                squares.removeAt(i);
    }
    return true;
}

bool MoveParser::parsePiecePostQualifier(const QString &qualifier, QList<BoardModel::BoardSquare> &squares) const
{
    // parse a following "square" qualifier, like "(B1)" or "(KKt7)"
    // `squares` is all the squares the piece could be on, reduce this to satisfy the qualifier
    QString squareQualifier;
    if (qualifier.startsWith('('))
    {
        if (!qualifier.endsWith(')'))
            return false;
        squareQualifier = qualifier.mid(1, qualifier.length() - 2);
    }
    if (squareQualifier.isEmpty())
        return false;

    // if the qualifier specified a row or any column(s)
    // remove any squares which do not match it
    int row;
    QList<int> cols;
    if (!parseSquareSpecifier(squareQualifier, row, cols))
        return false;
    for (int i = squares.length() - 1; i >= 0; i--)
        if ((row != -1 && squares.at(i).row != row) ||
                (!cols.isEmpty() && !cols.contains(squares.at(i).col)))
            squares.removeAt(i);

    return true;
}

bool MoveParser::parseSquareSpecifier(const QString &specifier, int &row, QList<int> &cols) const
{
    // parse a "square" specifier, used as the destination for a move like "P-K4" or in a "post-qualifier, like "R(R1)-Kt1" or "RxR(B7)"
    // set `row` to any row qualifier found, -1 => none
    // set `cols` to any column(s) qualifier found, [] => none

    row = -1;
    cols.clear();
    QString squareSpecifier(specifier);

    // parse the digit at the end for the row
    if (!squareSpecifier.isEmpty())
    {
        QChar digit = squareSpecifier.at(squareSpecifier.length() - 1);
        if (digit.isDigit())
        {
            squareSpecifier.chop(1);
            row = digit.toLatin1() - '1';
            if (row > 7 || row < 0)
                return false;
            // if Black player, row number counts in opposite direction
            if (player == Piece::Black)
                row = 7 - row;
        }
    }

    // parse the text at the start for the column(s)
    if (!squareSpecifier.isEmpty())
    {
        Piece::PieceName columnName;
        Piece::SideQualifier side;
        if (!parsePieceNameAndSide(squareSpecifier, columnName, side))
            return false;
        // cannot have "P" for column
        if (columnName == Piece::Pawn)
            return false;

        // figure which columns it could apply to
        cols = columnsForPieceAndSide(columnName, side);
        if (cols.length() == 0)
            return false;
    }

    return true;
}

bool MoveParser::parsePieceMoveFrom(QString lhs, QList<BoardModel::BoardSquare> &squaresFrom) const
{
    // parse piece and (optionally) square to move from, like "K" or "QB"
    // this produces a *list* of possible squares in `squaresFrom`, e.g. "P" could be any pawn
    squaresFrom.clear();

    // parse to get the piece, optional preceded and/or followed by "qualifiers", like "K" or "QB" or "KKtP" or "R(B1)"
    QString preQualifier, postQualifier;
    Piece::PieceName name;
    if (!parseFullPieceSpecifier(lhs, preQualifier, name, postQualifier))
        return false;

    // find all squares these pieces are on
    squaresFrom = model->findPieces(player, name);
    if (squaresFrom.isEmpty())
        return false;

    // see if there is a preceding "side-column" qualifier, like "K" or "QB"
    if (!preQualifier.isEmpty())
        if (!parsePiecePreQualifier(preQualifier, name, squaresFrom))
            return false;
    // see if there is a following "square" qualifier, like "(B1)"
    if (!postQualifier.isEmpty())
        if (!parsePiecePostQualifier(postQualifier, squaresFrom))
            return false;

    return true;
}

bool MoveParser::parseMoveTo(QString rhs, QList<BoardModel::BoardSquare> &squaresTo) const
{
    // parse square to move to, like "K4" or "QB4"
    // this produces a *list* of possible squares in `squaresTo`, e.g. "B4" could be either "KB4" or "QB4"
    squaresTo.clear();

    int row;
    QList<int> cols;
    if (!parseSquareSpecifier(rhs, row, cols))
        return false;
    // must specify a row and at least one possible column
    if (row == -1 || cols.isEmpty())
        return false;

    // build the list of possible squares to
    for (int col : cols)
        squaresTo.append({row, col});

    return true;
}

bool MoveParser::parseCaptureAt(QString rhs, QList<BoardModel::BoardSquare> &squaresTo, bool &enpassant) const
{
    // parse piece to capture, like "P" or "QBP"
    // this produces a *list* of possible squares in `squaresTo`, e.g. "BP" could be either "KBP" or "QBP"
    squaresTo.clear();
    enpassant = false;  // not enpassant

    // see if this is an "enpassant" capture ("ep") at the end
    QRegularExpressionMatch match;
    if (rhs.contains(QRegularExpression("^(.*)e\\.?p\\.?$", QRegularExpression::CaseInsensitiveOption), &match))
    {
        rhs = match.captured(1);
        enpassant = true;
    }

    // parse to get the piece, optional preceded and/or followed by "qualifiers", like "K" or "QB" or "KKtP" or "R(B1)"
    QString preQualifier, postQualifier;
    Piece::PieceName name;
    if (!parseFullPieceSpecifier(rhs, preQualifier, name, postQualifier))
        return false;

    // find all squares these pieces are on
    Piece::PieceColour opposingPlayer = Piece::opposingColour(player);
    squaresTo = model->findPieces(opposingPlayer, name);
    if (squaresTo.isEmpty())
        return false;

    // see if there is a preceding "side-column" qualifier, like "K" or "QB"
    if (!preQualifier.isEmpty())
        if (!parsePiecePreQualifier(preQualifier, name, squaresTo))
            return false;
    // see if there is a following "square" qualifier, like "(B1)"
    if (!postQualifier.isEmpty())
        if (!parsePiecePostQualifier(postQualifier, squaresTo))
            return false;

    return true;
}

QList<BoardModel::BoardSquareFromTo> MoveParser::resolveSquaresFromTo(
        const QList<BoardModel::BoardSquare> &squaresFrom, const QList<BoardModel::BoardSquare> &squaresTo,
        bool capture, bool enpassant, bool check) const
{
    // given a list of possible squares to move from and squares to move to
    // resolve to a list of possible from-tos
    // `capture` tells whether it it is a capture move (possibly enpassant), else it is a move move
    // `check` tells whether the move/capture results in check
    Q_UNUSED(check)

    QList<BoardModel::BoardSquareFromTo> possibles;
    if (squaresFrom.isEmpty() || squaresTo.isEmpty())
        return possibles;

    // go through each square from
    QList<BoardModel::BoardSquare> squaresOpposingKing(model->findPieces(Piece::opposingColour(player), Piece::King));
    for (const auto squareFrom : squaresFrom)
    {
        const Piece *piece = model->pieceAt(squareFrom);
        Q_ASSERT(piece);
        Q_ASSERT(piece->colour == player);
        // go through each square to
        // if the piece at squareFrom could move to squareTo append that pair to possibles
        for (const auto squareTo : squaresTo)
        {
            // test for raw move to/capture at
            if (!model->couldMoveFromTo(squareFrom, squareTo, capture, enpassant))
                continue;
            // if `check` is true, test for that move resulting in check on opposing King
            if (check && squaresOpposingKing.length() == 1)
                if (!model->couldMoveFromTo(*piece, squareTo, squaresOpposingKing[0], true, false))
                    continue;
            possibles.append({squareFrom, squareTo});
        }
    }
    return possibles;
}



MoveUndoCommand::MoveUndoCommand(BoardModel *boardModel, Piece::PieceColour player, const QString &text, const QList<MoveParser::ParsedMove> &moves)
{
    Q_ASSERT(boardModel);
    this->_boardModel = boardModel;
    this->_player = player;
    this->_moveText = text;
    this->_moves = moves;
    this->setText("Last Move");
}

/*virtual*/ void MoveUndoCommand::redo() /*virtual*/
{
    // do a MoveUndoCommand, either first time or after an undo
    _boardModel->doUndoableMoveCommand(*this);
}

/*virtual*/ void MoveUndoCommand::undo() /*override*/
{
    // undo a MoveUndoCommand, after redo have previously been called
    _boardModel->undoUndoableMoveCommand(*this);
}

