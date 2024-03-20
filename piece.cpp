#include "piece.h"

Piece::Piece(PieceColour colour, PieceName name, SideQualifier side /*= NoSide*/)
{
    this->colour = colour;
    this->name = name;
    this->side = side;
}
