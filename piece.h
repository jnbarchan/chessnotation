#ifndef PIECE_H
#define PIECE_H


class Piece
{
public:
    enum PieceColour { White, Black };
    enum PieceName { Bishop, King, Knight, Pawn, Queen, Rook };
    enum SideQualifier { NoSide, KingSide, QueenSide };

public:
    Piece(PieceColour colour, PieceName name, SideQualifier side = NoSide);

    inline static PieceColour opposingColour(PieceColour colour) { return (colour == White) ? Black : White; }
    PieceColour opposingColour() const { return opposingColour(colour); }
    inline bool isWhite() const { return (colour == White); }
    inline bool isBlack() const { return (colour == Black); }

public:
    PieceColour colour;
    PieceName name;
    SideQualifier side;
};

#endif // PIECE_H
