#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextStream>
#include <QTimer>

class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QSpacerItem;
class QTableView;

#include "piece.h"

class BoardModel;
class BoardScene;
class OpenedGameRunner;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    BoardModel *boardModel;
    BoardScene *boardScene;
    QTableView *moveHistoryView;
    QHBoxLayout *hblytEnterMove;
    QLineEdit *leEnterMove;
    QSpacerItem *spcrRightEnterMove;
    QLabel *lblParserMessage;
    QMenu *mainMenu, *runMenu;
    QFrame *runButtonsFrame;
    QAction *undoAction, *redoAction;
    OpenedGameRunner *openedGameRunner;
    QString _appRootPath;
    const QString appRootPath();
    void setupUi();
    void setEnterMoveError(bool error);
    void setEnterMovePosition(Piece::PieceColour player);
    Piece::PieceColour activePlayer() const;
    bool parseAndMakeMove(const QString &text);

private slots:
    void parserMessage(const QString &msg);
    void stepOneMove(const QString &token);
    void actionNewGame();
    void actionOpenGame();
    void actionSaveGame();
    void actionPieceSet();
    void actionAbout();
    void boardModelStartedNewGame();
    void moveEntered();
    void moveMade(const QString &text);
};


class OpenedGameRunner : public QObject
{
    Q_OBJECT

public:
    OpenedGameRunner(QMenu *runMenu, QFrame *runButtonsFrame, BoardModel *boardModel, QWidget *parent = nullptr);

    void setupUi();
    void readFile(QTextStream &ts);
    void moveToNextToken();

private:
    BoardModel *boardModel;
    QMenu *runMenu;
    QFrame *runButtonsFrame;
    QAction *restartAction, *stepAction, *runPauseAction, *runToEndAction, *returnToReachedAction;
    QStringList allTokens;
    int currentTokenIndex;
    QTimer runStepTimer;
    bool doStepOneMove();

public slots:
    void runStepTimerStop();
    void clear();

private slots:
    void updateMenuEnablement();
    void actionRestart();
    void actionStep();
    void actionRunPause();
    void actionRunToEnd();
    void actionReturnToReached();

signals:
    void stepOneMove(const QString &token);
};

#endif // MAINWINDOW_H
