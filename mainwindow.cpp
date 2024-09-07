#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableView>
#include <QTextStream>
#include <QToolButton>

#include "boardmodel.h"
#include "boardscene.h"
#include "boardview.h"
#include "piecesetdialog.h"
#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setGeometry(100, 100, 800, 800);  // default size

    // create the board model
    this->boardModel = new BoardModel(this);
    // create the graphics scene
    this->boardScene = new BoardScene(boardModel, this);

    setupUi();

    // signal connections
    connect(boardModel, &BoardModel::startedNewGame, this, &MainWindow::boardModelStartedNewGame);
    connect(boardModel, &BoardModel::parserMessage, this, &MainWindow::parserMessage);
    connect(boardModel, &BoardModel::lastMoveMade, this, &MainWindow::moveMade);
    connect(leEnterMove, &QLineEdit::textChanged, this, [this]() { setEnterMoveError(false); lblParserMessage->clear(); } );
    connect(leEnterMove, &QLineEdit::returnPressed, this, &MainWindow::moveEntered);
    MoveHistoryModel *moveHistoryModel(boardModel->moveHistoryModel());
    connect(moveHistoryModel, &MoveHistoryModel::moveAppended, moveHistoryView, &QTableView::scrollToBottom);
    connect(moveHistoryModel, &MoveHistoryModel::lastMoveRemoved, moveHistoryView, &QTableView::scrollToBottom);
    connect(openedGameRunner, &OpenedGameRunner::stepOneMove, this, &MainWindow::stepOneMove);
    connect(undoAction, &QAction::triggered, openedGameRunner, &OpenedGameRunner::runStepTimerStop);

    // start new game
    boardModel->newGame();
}

MainWindow::~MainWindow()
{
}

const QString MainWindow::appRootPath()
{
    // root directory for locating images/sample game files
    if (_appRootPath.isEmpty())
    {
        // look relative to where the *executable* directory is
        // if that is the right directory return that (i.e. application has been deployed)
        // if not assume it is the development "build" directory and return "../.." from there
        QDir dir(QCoreApplication::applicationDirPath());
        if (dir.exists("images") && dir.exists("samplegames"))
            _appRootPath = dir.absolutePath();
        else if (dir.cd("../.."))
            if (dir.exists("images") && dir.exists("samplegames"))
                _appRootPath = dir.absolutePath();
    }
    return _appRootPath;
}

void MainWindow::setupUi()
{
    // set centralwidget layout
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    setCentralWidget(widget);

    // create the menus
    this->mainMenu = new QMenu("Menu", this);
    menuBar()->addMenu(mainMenu);
    mainMenu->addAction("New Game", this, &MainWindow::actionNewGame);
    mainMenu->addSeparator();
    mainMenu->addAction("Open Game...", this, &MainWindow::actionOpenGame);
    this->runMenu = mainMenu->addMenu("Play Opened Game");
    mainMenu->addAction("Save Game...", this, &MainWindow::actionSaveGame);
    mainMenu->addSeparator();
    this->undoAction = boardModel->createUndoMoveAction(this);
    undoAction->setIcon(QIcon(appRootPath() + "/images/undo.png"));
    undoAction->setShortcut(QKeySequence::Undo);
    mainMenu->addAction(undoAction);
    this->redoAction = boardModel->createRedoMoveAction(this);
    redoAction->setIcon(QIcon(appRootPath() + "/images/redo.png"));
    redoAction->setShortcut(QKeySequence::Redo);
    mainMenu->addAction(redoAction);
    mainMenu->addSeparator();
    mainMenu->addAction("Piece Set...", this, &MainWindow::actionPieceSet);
    mainMenu->addSeparator();
    mainMenu->addAction("About", this, &MainWindow::actionAbout);
    mainMenu->addSeparator();
    mainMenu->addAction("Exit", qApp, &QApplication::quit);

    // load the pieces
    const QString dirPath = appRootPath() + "/images/piece_set_1";
    boardScene->loadPieceImages(dirPath);

    boardScene->setSceneRect(0, 0, 800, 800);
    // create the graphics view, as left-hand pane
    QVBoxLayout *leftLayout = new QVBoxLayout;
    layout->addLayout(leftLayout);
    BoardView *boardView = new BoardView;
    boardView->setScene(boardScene);
    boardView->fitInView(boardScene->sceneRect(), Qt::KeepAspectRatio);
    leftLayout->addWidget(boardView);

    // create the area to input/output moves, as right-hand pane
    QWidget *rightPane = new QWidget;
    rightPane->setFixedWidth(250);
    layout->addWidget(rightPane);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPane);

    // create the move history view
    this->moveHistoryView = new QTableView;
    moveHistoryView->horizontalHeader()->hide();
    moveHistoryView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    moveHistoryView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    rightLayout->addWidget(moveHistoryView);
    moveHistoryView->setModel(boardModel->moveHistoryModel());

    // create the run and undo frames and buttons
    QHBoxLayout *hlayout = new QHBoxLayout;
    rightLayout->addLayout(hlayout);
    this->runButtonsFrame = new QFrame;
    runButtonsFrame->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    runButtonsFrame->setSizePolicy(QSizePolicy::Maximum, runButtonsFrame->sizePolicy().verticalPolicy());
    hlayout->addWidget(runButtonsFrame);
    QFrame *undoButtonsFrame = new QFrame;
    undoButtonsFrame->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    undoButtonsFrame->setSizePolicy(QSizePolicy::Maximum, runButtonsFrame->sizePolicy().verticalPolicy());
    hlayout->addWidget(undoButtonsFrame);
    undoButtonsFrame->setLayout(new QHBoxLayout);
    for (QAction *action : { undoAction, redoAction } )
    {
        QToolButton *btn = new QToolButton();
        btn->setMinimumWidth(30);
        btn->setDefaultAction(action);
        undoButtonsFrame->layout()->addWidget(btn);
    }

    // create the "opened game runner", handing it control of `runMenu` & `runButtonsFrame`
    this->openedGameRunner = new OpenedGameRunner(runMenu, runButtonsFrame, boardModel, this);

    // create the QLineEdit for entering moves
    QWidget *move = new QWidget;
    rightLayout->addWidget(move);
    this->hblytEnterMove = new QHBoxLayout(move);
    QLabel *label = new QLabel("Move:");
    hblytEnterMove->addWidget(label);
    this->leEnterMove = new EnterMoveLineEdit;
    leEnterMove->setFixedWidth(100);
    hblytEnterMove->addWidget(leEnterMove);
    this->spcrRightEnterMove = new QSpacerItem(1, 1, QSizePolicy::Expanding);
    hblytEnterMove->addSpacerItem(spcrRightEnterMove);

    // create the QLabel for parser messages
    this->lblParserMessage = new QLabel;
    lblParserMessage->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    lblParserMessage->setWordWrap(true);
    QFont font(lblParserMessage->font());
    int size = font.pointSize();
    size = (size >= 10) ? size - 2 : (size >= 7) ? size - 1 : size;
    font.setPointSize(size);
    lblParserMessage->setFont(font);
    lblParserMessage->setTextFormat(Qt::PlainText);
    lblParserMessage->setFixedHeight(100);
    lblParserMessage->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    lblParserMessage->setLineWidth(2);
    rightLayout->addWidget(lblParserMessage);
}

void MainWindow::setEnterMoveError(bool error)
{
    // show entered move in red if error, else reset to default colour
    if (error)
        leEnterMove->setStyleSheet("color: red;");
    else
        leEnterMove->setStyleSheet("");
}

void MainWindow::setEnterMovePosition(Piece::PieceColour player)
{
    // move the position of `leEnterMove` to correspond to `player`
    spcrRightEnterMove->changeSize(1, 1, (player == Piece::White) ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    hblytEnterMove->invalidate();
}

Piece::PieceColour MainWindow::activePlayer() const
{
    // return the active player (player to move)
    // this is taken from the move history model
    return boardModel->moveHistoryModel()->playerToMove();
}

bool MainWindow::parseAndMakeMove(const QString &text)
{
    // (attempt to) parse the text of a move
    // if successful make the move
    lblParserMessage->clear();
    // try to parse
    if (!boardModel->parseAndMakeMove(activePlayer(), text))
    {
        // show the move in error
        setEnterMoveError(true);
        return false;
    }

    // move made, end of turn
    return true;
}

/*slot*/ void MainWindow::parserMessage(const QString &msg)
{
    // slot for any messages emitted by BoardModel::MoveParser
    lblParserMessage->setText(msg);
}

/*slot*/ void MainWindow::stepOneMove(const QString &token)
{
    // slot for OpenedGameRunner::stepOneMove()
    // set the enter move line edit to the token and (try to) parse it and make the move
    leEnterMove->setText(token);
    if (parseAndMakeMove(token))
        openedGameRunner->moveToNextToken();
}

/*slot*/ void MainWindow::actionNewGame()
{
    // action for "New Game"
    openedGameRunner->clear();
    boardModel->newGame();    // causes boardModelStartedNewGame() to be called
}

/*slot*/ void MainWindow::actionOpenGame()
{
    // action for "Open Game"

    // (try to) open the file for read
    const QString dirPath = appRootPath() + "/samplegames";
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", dirPath);
    if (filePath.isEmpty())
        return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::information(this, "Failed to Open File", QString("%1: %2").arg(file.fileName()).arg(file.errorString()));
        return;
    }

    // start a new game
    actionNewGame();

    // get `openedGameRunner` to read the file, splitting into tokens
    QTextStream ts(&file);
    openedGameRunner->readFile(ts);
    file.close();
}

/*slot*/ void MainWindow::actionSaveGame()
{
    // action for "Save Game"

    // (try to) open the file for write
    const QString rootPath = appRootPath() + "/samplegames";
    QString filePath = QFileDialog::getSaveFileName(this, "Save File", rootPath);
    if (filePath.isEmpty())
        return;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::information(this, "Failed to Open File", QString("%1: %2").arg(file.fileName()).arg(file.errorString()));
        return;
    }

    // save the move history to the file
    QTextStream ts(&file);
    boardModel->saveMoveHistory(ts);
    file.close();
}

/*slot*/ void MainWindow::actionPieceSet()
{
    // action for "Piece Set"
    PieceSetDialog dlg(boardScene, appRootPath(), this);
    dlg.exec();
}

/*slot*/ void MainWindow::actionAbout()
{
    // action for "About"
   QMessageBox::about(this, "About Chess",
                      "Program to show a chessboard and allow input of moves in \"Descriptive\" notation.");
}

void MainWindow::boardModelStartedNewGame()
{
    // slot for when the board model has (set up and) started a new game
    leEnterMove->clear();
    setEnterMovePosition(activePlayer());
}

/*slot*/ void MainWindow::moveEntered()
{
    // slot for completing entering a move
    // get text, removing *all* whitespace
    QString text = leEnterMove->text().simplified();
    text.replace(" ", "");
    if (text.isEmpty())
        return;

    // try to parse, and make move if successful
    if (!parseAndMakeMove(text))
        return;

    // autosave game so far after each move entered (e.g. in case there is a crash can receiver moves typed in)
    QString filePath(QDir::tempPath() + "/chess.sav");
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream ts(&file);
        boardModel->saveMoveHistory(ts);
        file.close();
    }
}

/*slot*/ void MainWindow::moveMade(const QString &text)
{
    // slot for move has been made
    // clear out text in line edit
    Q_UNUSED(text);
    leEnterMove->clear();
    // move the line edit to correspond to the (new) active player
    setEnterMovePosition(activePlayer());
}


/*virtual*/ bool EnterMoveLineEdit::event(QEvent *e) /*override*/
{
    // the enter move line edit needs to ensure **Return** gets to it (for `returnPressed()` signal)
    // but because **Return** is used a shortcut when move-stepping is active that would go there
    // so we handle `ShortcutOverride` event to forbid that being propagated away from this line edit
    if (e->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
        if (keyEvent->key() == Qt::Key_Return)
        {
            e->accept();
            return true;
        }
    }
    return QLineEdit::event(e);
}


OpenedGameRunner::OpenedGameRunner(QMenu *runMenu, QFrame *runButtonsFrame, BoardModel *boardModel, QWidget *parent)
    : QObject(parent)
{
    this->runMenu = runMenu;
    this->runButtonsFrame = runButtonsFrame;
    this->boardModel = boardModel;

    setupUi();

    connect(boardModel, &BoardModel::undoStackIndexChanged, this, &OpenedGameRunner::updateMenuEnablement);
    connect(&runStepTimer, &QTimer::timeout, this, &OpenedGameRunner::actionStep);
}

void OpenedGameRunner::setupUi()
{
    // setup the "Play Opened Game" submenu
    QStyle &style(*runMenu->style());
    restartAction = runMenu->addAction(style.standardIcon(QStyle::SP_MediaSkipBackward), "Restart", this, &OpenedGameRunner::actionRestart);
    stepAction = runMenu->addAction(style.standardIcon(QStyle::SP_ArrowForward), "Step", Qt::Key_Return, this, &OpenedGameRunner::actionStep);
    runPauseAction = runMenu->addAction(style.standardIcon(QStyle::SP_MediaPlay), "Run", Qt::Key_Space, this, &OpenedGameRunner::actionRunPause);
    runToEndAction = runMenu->addAction(style.standardIcon(QStyle::SP_MediaSkipForward), "Run to End", this, &OpenedGameRunner::actionRunToEnd);
    returnToReachedAction = runMenu->addAction("Return to Reached", this, &OpenedGameRunner::actionReturnToReached);
    // and the corresponding buttons in `runButtonsFrame`
    runButtonsFrame->setLayout(new QHBoxLayout);
    for (QAction *action : { restartAction, stepAction, runPauseAction, runToEndAction, /*returnToReachedAction*/ } )
    {
        QToolButton *btn = new QToolButton();
        btn->setMinimumWidth(30);
        btn->setDefaultAction(action);
        runButtonsFrame->layout()->addWidget(btn);
    }
    clear();
}

/*slot*/ void OpenedGameRunner::updateMenuEnablement()
{
    // enable/disable menu items corresponding to current state
    runMenu->setEnabled(allTokens.count() > 0);
    runPauseAction->setText(runStepTimer.isActive() ? "Pause" : "Run");
    runPauseAction->setIcon(runMenu->style()->standardIcon(runStepTimer.isActive() ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    // we can only "continue" making moves if not at the end of moves and have not changed state since the last move was made
    bool atEnd = (currentTokenIndex >= allTokens.count());
    bool isClean = boardModel->undoStackIsClean();
    bool canContinue = (!atEnd && isClean);
    stepAction->setEnabled(canContinue);
    runPauseAction->setEnabled(canContinue);
    runToEndAction->setEnabled(canContinue);
    returnToReachedAction->setEnabled(boardModel->undoStackCanRestoreToClean());
}

void OpenedGameRunner::readFile(QTextStream &ts)
{
    // read the file content, split into tokens on any whitespace
    runStepTimer.stop();
    QString fileContent = ts.readAll();
    this->allTokens = fileContent.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    // if there is a turn number before white moves remove it
    for (int i = 0; i < allTokens.count(); i++)
        if (i % 2 == 0 && allTokens.at(i).contains(QRegularExpression("^\\d+\\.?$")))
            allTokens.removeAt(i--);
    currentTokenIndex = 0;
    updateMenuEnablement();
}

void OpenedGameRunner::moveToNextToken()
{
    // previous token has been successfully parsed and move made
    // so move to next token (which could be one beyond the end of the tokens)
    // called from `MainWindow::stepOneMove()`
    if (currentTokenIndex < allTokens.count())
        currentTokenIndex++;
}

bool OpenedGameRunner::doStepOneMove()
{
    // emit the `stepOneMove()` signal, causing a move to be made
    // calls `MainWindow::stepOneMove()`
    // which in turn calls `moveToNextToken()` on success to move to the next token
    // check whether the move was successfully made and return that

    // we should not get here if the undoStack is not clean
    // if it is unclean we are in a bad state, and cannot afford to do step moves
    Q_ASSERT(boardModel->undoStackIsClean());

    int oldIndex = currentTokenIndex;
    emit stepOneMove(allTokens[currentTokenIndex]);
    // if `currentTokenIndex` has not changed move has failed
    bool success = (currentTokenIndex != oldIndex);
    if (success)
        boardModel->undoStackSetClean();
    return success;
}

/*slot*/ void OpenedGameRunner::runStepTimerStop()
{
    // stop any running step timer
    runStepTimer.stop();
}

/*slot*/ void OpenedGameRunner::clear()
{
    // clear any opened game
    runStepTimer.stop();
    allTokens.clear();
    currentTokenIndex = 0;
    updateMenuEnablement();
}

/*slot*/ void OpenedGameRunner::actionRestart()
{
    runStepTimer.stop();
    boardModel->newGame();
    currentTokenIndex = 0;
    updateMenuEnablement();
}

/*slot*/ void OpenedGameRunner::actionStep()
{
    // emit the `stepOneMove()` signal
    // calls `MainWindow::stepOneMove()`
    // which in turn calls `moveToNextToken()` on success to move to the next token
    if (currentTokenIndex >= allTokens.count())
        return;
    if (!doStepOneMove())
        runStepTimer.stop();
    updateMenuEnablement();
}

/*slot*/ void OpenedGameRunner::actionRunPause()
{
    // toggle whether `runStepTimer` is running or stopped
    if (runStepTimer.isActive())
        runStepTimer.stop();
    else
        runStepTimer.start(1000);
    updateMenuEnablement();
}

/*slot*/ void OpenedGameRunner::actionRunToEnd()
{
    // repeatedly emit the `stepOneMove()` signal
    // till we reach the end, or a move fails
    runStepTimer.stop();
    updateMenuEnablement();
    while (currentTokenIndex < allTokens.count())
        if (!doStepOneMove())
            break;
    updateMenuEnablement();
}

/*slot*/ void OpenedGameRunner::actionReturnToReached()
{
    // return to where user had reached in the opened game
    // this could be redoing moves which have been undone, or undoing new moves typed in by user
    runStepTimer.stop();
    boardModel->undoStackRestoreToClean();
    updateMenuEnablement();
}
