#include "previewwindow.h"
#include "autodrawer.h"
#include "consolewindow.h"
#include "ui_previewwindow.h"
#include "messagewindow.h"
#include <iostream>
#include <fstream>
#include <QShortcut>
#include <QtConcurrent>
#include <thread>
#include <chrono>
#include <future>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QFile>
#include <QJsonParseError>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>
#include <algorithm>

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 4920;

#ifdef _WIN32
    #define WINVER 0x0500
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif  __linux__
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    Display *display = XOpenDisplay (NULL);
#elif __APPLE__

#endif

bool cursorHeld = false;
bool loopRunning = true;
bool stopAutodraw;
int moveInterval;
int clickDelay;
QImage image;
int path8 = 12345678;
using namespace Qt;
QWidget* MainWin;

std::vector<std::vector<int>> pixelArray;

bool cont;
bool CloseRequested = false;
bool Paused = false;

auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AutoDraw";

int fd;
Display *dpy;

PreviewWindow::PreviewWindow(QImage dimage, int interval, int delay, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PreviewWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);
    setParent(0);
    setMouseTracking(true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    ui->ShownImage->setPixmap(QPixmap::fromImage(dimage));
    resize(dimage.width(), dimage.height()+21);
    ui->ShownImage->setFixedWidth(dimage.width());
    ui->ShownImage->setFixedHeight(dimage.height());
    ui->Header->setFixedWidth(dimage.width());
    ui->Background->setFixedWidth(dimage.width());
    ui->Background->setFixedHeight(dimage.height()+21);
    ui->LastPOS->setShortcut(Qt::CTRL);
    ui->Stop->setShortcut(Qt::ALT);
    ui->Draw->setShortcut(QKeySequence(Qt::SHIFT));
    moveInterval = interval;
    clickDelay = delay;
    image = dimage;
    pixelArray.clear();
    pixelArray.resize(image.width(), std::vector<int>(image.height(), 0));
    MainWin = parent;
    reloadThemes();
    loopRunning = true;
    cursorHeld = false;
    //parent->show();
    if (!QFile::exists(path+"/hotkey.py")){
        std::ofstream MyFile((path+"/hotkey.py").toStdString());
        // I really do not want a super long string
        MyFile << "from pynput import keyboard\nfrom pynput.keyboard import Key\nimport sys\n";
        MyFile << "def stop():\n    listener.stop()\n    sys.exit(0)\n";
        MyFile << "def on_press(key):\n    if(key==Key.shift and sys.argv[1] == \"start\"):\n";
        MyFile << "        stop()\n    elif(key==Key.ctrl and sys.argv[1] == \"lock\"):\n        stop()";
        MyFile << "\n    elif(key==Key.alt and sys.argv[1] == \"stop\"):\n        stop()";
        MyFile << "\nwith keyboard.Listener(on_press=on_press) as listener:\n    listener.join()";
        MyFile.close();
    }
    QFuture<void> future = QtConcurrent::run([=]() {
        while (loopRunning){
            move(QCursor::pos().x() - ui->ShownImage->width()/2, QCursor::pos().y() - ui->ShownImage->height()/2);
        }
    });
        #ifdef __linux__


        #endif
    QFuture<void> start = QtConcurrent::run([=]() {
    #ifdef _WIN32


    #endif
        if(pyCode("start") && !stopAutodraw) {
            Draw();
        };
    });
    QFuture<void> stop = QtConcurrent::run([=]() {
        if(pyCode("stop")) {on_pushButton_2_released(); new ConsoleWindow("Stopped drawing.");}
    });
    QFuture<void> lockpos = QtConcurrent::run([=]() {
        if(pyCode("lock")) {lockPos(); new ConsoleWindow("Locked window position.");}
    });
}

static void sendMessage(QString a, int b, QWidget *t){
    //1 for info, 2 for error, 3 for alert
    MessageWindow *w = new MessageWindow(a, b, t);
    w->show();
}

void PreviewWindow::reloadThemes(){

    QFile inFile(path+"/user.cfg");
    inFile.open(QIODevice::ReadOnly|QIODevice::Text);
    QByteArray data = inFile.readAll();

    QJsonParseError errorPtr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &errorPtr);
    QJsonObject rootObj2 = doc.object();
    auto theme = rootObj2.value("theme").toString();
    path8 = rootObj2.value("pattern").toInt();
    inFile.close();

    QJsonObject preview;
    QJsonObject rootObj;
    if (theme == "dark") {
        extern QString darkJson;
        QByteArray br = darkJson.toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(br);
        rootObj = doc.object();
        preview = rootObj["settings"].toObject();
    } else if (theme == "light"){
        extern QString lightJson;
        QByteArray br = lightJson.toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(br);
        rootObj = doc.object();
        preview = rootObj["settings"].toObject();
    } else if (QFile::exists(path+"/themes/"+theme+".drawtheme") ){
        QFile inFile2(path+"/themes/"+theme+".drawtheme");
        inFile2.open(QIODevice::ReadOnly|QIODevice::Text);
        QByteArray themeData = inFile2.readAll();
        QJsonParseError errorPtr;
        QJsonDocument docT = QJsonDocument::fromJson(themeData, &errorPtr);
        rootObj = docT.object();
        preview = rootObj["settings"].toObject();
        inFile2.close();
    }
    ui->Header->setStyleSheet("color: "+preview["text"].toString());
    ui->Background->setStyleSheet("border-radius: 25px; background: "+preview["background"].toString());
}

bool PreviewWindow::pyCode(std::string str){
    QStringList arguments;
    arguments << path+"/hotkey.py" << str.c_str();
    QProcess *myProcess = new QProcess(this);
    myProcess->start("python3", arguments);
    myProcess->waitForFinished(-1);
    if(myProcess->exitCode() == 0) return true; else return false;
}

void PreviewWindow::setCursor(int x, int y){
    //This code is cross-platform, but does not work with games.
    //QCursor::setPos(QPoint(x,y));
    #ifdef _WIN32


    #elif  __linux__
        XEvent event;
        memset (&event, 0, sizeof (event));
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        event.xbutton.subwindow = DefaultRootWindow (display);
        while (event.xbutton.subwindow)
          {
            event.xbutton.window = event.xbutton.subwindow;
            XQueryPointer (display, event.xbutton.window,
                   &event.xbutton.root, &event.xbutton.subwindow,
                   &event.xbutton.x_root, &event.xbutton.y_root,
                   &event.xbutton.x, &event.xbutton.y,
                   &event.xbutton.state);
          }

        XWarpPointer (display, None, None, 0,0,0,0, x-QCursor::pos().x(), y-QCursor::pos().y());
        XFlush (display);
    #endif
}

void PreviewWindow::clickCursor(){
    //This code is not cross platform, so you have to run different code on different OS'.
    #ifdef _WIN32

    #elif  __linux__
        // Create and setting up the event
        XEvent event;
        memset (&event, 0, sizeof (event));
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        event.xbutton.subwindow = DefaultRootWindow (display);
        while (event.xbutton.subwindow)
          {
            event.xbutton.window = event.xbutton.subwindow;
            XQueryPointer (display, event.xbutton.window,
                   &event.xbutton.root, &event.xbutton.subwindow,
                   &event.xbutton.x_root, &event.xbutton.y_root,
                   &event.xbutton.x, &event.xbutton.y,
                   &event.xbutton.state);
          }
        // Press
        event.type = ButtonPress;
        if (XSendEvent (display, PointerWindow, True, ButtonPressMask, &event) == 0)
          fprintf (stderr, "Error to send the event!\n");
        XFlush (display);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        // Release
        event.type = ButtonRelease;
        if (XSendEvent (display, PointerWindow, True, ButtonReleaseMask, &event) == 0)
          fprintf (stderr, "Error to send the event!\n");
        XFlush (display);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    #elif __APPLE__

    #endif
}

void PreviewWindow::holdCursor(){
    //This code is not cross platform, so you have to run different code on different OS'.
    #ifdef _WIN32

    #elif  __linux__
        XEvent event;
        memset (&event, 0, sizeof (event));
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        event.xbutton.subwindow = DefaultRootWindow (display);
        while (event.xbutton.subwindow)
          {
            event.xbutton.window = event.
            xbutton.subwindow;
            XQueryPointer (display, event.xbutton.window,
                   &event.xbutton.root, &event.xbutton.subwindow,
                   &event.xbutton.x_root, &event.xbutton.y_root,
                   &event.xbutton.x, &event.xbutton.y,
                   &event.xbutton.state);
          }
        // Press
        event.type = ButtonPress;
        if (XSendEvent (display, PointerWindow, True, ButtonPressMask, &event) == 0)
          fprintf (stderr, "Error to send the event!\n");
        XFlush (display);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    #elif __APPLE__

    #endif
}

void PreviewWindow::releaseCursor(){
    //This code is not cross platform, so you have to run different code on different OS'.
    #ifdef _WIN32

    #elif  __linux__
        XEvent event;
        memset (&event, 0, sizeof (event));
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        event.xbutton.subwindow = DefaultRootWindow (display);
        while (event.xbutton.subwindow)
          {
            event.xbutton.window = event.xbutton.subwindow;
            XQueryPointer (display, event.xbutton.window,
                   &event.xbutton.root, &event.xbutton.subwindow,
                   &event.xbutton.x_root, &event.xbutton.y_root,
                   &event.xbutton.x, &event.xbutton.y,
                   &event.xbutton.state);
          }
        // Release
        event.type = ButtonRelease;
        if (XSendEvent (display, PointerWindow, True, ButtonReleaseMask, &event) == 0)
          fprintf (stderr, "Error to send the event!\n");
        XFlush (display);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    #elif __APPLE__

    #endif
}

int PreviewWindow::getPA(int x, int y){
    return pixelArray[x][y];
}

void PreviewWindow::setPA(int x, int y, int value){
    pixelArray[x][y] = value;
}

PreviewWindow::~PreviewWindow()
{
    delete ui;
}
void PreviewWindow::closeDraw(int a){
    //0 is stopped, 1 is success
    #ifdef __linux__
            XCloseDisplay (display);
    #endif
    if (a == 1){
        new ConsoleWindow("Drawing finished.");
        sendMessage("Drawing finished", 4, MainWin);
        this->close();
    } else{
        new ConsoleWindow("Drawing stopped.");
        sendMessage("Drawing stopped", 5, MainWin);
        this->close();
    }
}

void PreviewWindow::lockPos(){
    //To be done when the save config code is done
}

void PreviewWindow::on_pushButton_2_released()
{
    if (loopRunning) stopAutodraw = true; else {
        MainWin->show();
    }
}
std::vector<QPoint> stack;
std::vector<int> pathStr;
void PreviewWindow::Draw()
{
    QFile inFile(path+"/user.cfg");
    inFile.open(QIODevice::ReadOnly|QIODevice::Text);
    QByteArray data = inFile.readAll();

    QJsonParseError errorPtr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &errorPtr);
    QJsonObject rootObj2 = doc.object();
    auto printer = rootObj2.value("printer").toBool();
    inFile.close();
    new ConsoleWindow("Started drawing {\n   Interval: "+QString::number(moveInterval)+"\n   Click Delay: "+QString::number(clickDelay)+"\n}");

    loopRunning = false;
    this->hide();
    int x = QCursor::pos().x() - (image.width()/2);
    int y = QCursor::pos().y() - (image.height()/2);
    if (printer){
        //printer mode
        for (int yImg = 0; yImg < image.height(); ++yImg) {
            if (stopAutodraw) break;
            QRgb *scanLine = (QRgb*)image.scanLine(yImg);
            if (yImg % 2 == 0){
                for (int xImg = 0; xImg < image.width(); ++xImg) {
                    if (stopAutodraw) break;
                    QRgb pixel = *scanLine;
                    uint ci = uint(qGray(pixel));
                    if (ci <= 254){
                        setCursor(x+xImg, y+(yImg));
                        if (!cursorHeld) {holdCursor(); cursorHeld = true;}
                        std::this_thread::sleep_for(std::chrono::microseconds(moveInterval));
                    }
                    else{
                        if (cursorHeld) {releaseCursor(); cursorHeld = false;}
                    }
                    ++scanLine;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(clickdelay));
            }
            else{
                --scanLine;
                for (int xImg = image.width(); xImg --> 0;) {
                    if (stopAutodraw) break;
                    QRgb pixel = *scanLine;
                    uint ci = uint(qGray(pixel));
                    if (ci <= 254){
                        setCursor(x+xImg, y+(yImg));
                        if (!cursorHeld) {holdCursor(); cursorHeld = true;}
                        std::this_thread::sleep_for(std::chrono::microseconds(moveInterval));
                    }
                    else{
                        if (cursorHeld) {releaseCursor(); cursorHeld = false;}
                    }
                    --scanLine;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(clickdelay));
            }
        }
    }
    else{
        pixelArray.resize(x, std::vector<int>(y, 0));
        cont = true;
        //dodgy code somewhat
        for(int i = 0; i< QString::number(path8).length(); i++)
        {
            if (QString::number(path8).at(i) != " ")
                pathStr.push_back(QString::number(path8).at(i).toLatin1());
        }
        //end dodgy code

        //Scan
        new ConsoleWindow("Starting pixel scan");
        for (int xImg = 1; xImg < image.height(); ++xImg) {
            if (stopAutodraw) break;
            QRgb *scanLine2 = (QRgb*)image.scanLine(xImg);
            for (int yImg = 1; yImg < image.width(); ++yImg) {
                if (stopAutodraw) break;
                QRgb pixel2 = *scanLine2;
                uint ci2 = uint(qGray(pixel2));
                //new ConsoleWindow(QString::number(ci2));
                if (ci2 <= 254){
                    setPA(xImg, yImg, 1);
                }
                else{
                    setPA(xImg, yImg, 0);
                }
            }
        }
        new ConsoleWindow("Scan successful");
        //USE THE BELOW CODE TO TEST OUT WHAT THE ARRAY LOOKS LIKE IN IMAGE FORM
/*
        QImage image2(image.width(), image.height(), QImage::Format_RGB32);
        for (int i=0;i<image2.width();++i) {
            for (int j=0;j<image2.height();++j) {
                QRgb value = getPA(i, j);
                if (value % 5 == 0) value = 255; else value = 0;
                //new ConsoleWindow(QString::number(getPA(i, j)));
                image.setPixel(i, j, value);
                value = 0;
            }
        }
        ui->ShownImage->setPixmap(QPixmap::fromImage(image));
        this->show();
        new ConsoleWindow("Image test mode on, showing user scanned image.");
        return;
*/
        //END OF TEST CODE
        //Draw
        new ConsoleWindow("For loop");
        for (int yImg = 0; yImg < image.height(); ++yImg) {
            if (stopAutodraw) break;
            QRgb *scanLine = (QRgb*)image.scanLine(yImg);
            for (int xImg = 0; xImg < image.width(); ++xImg) {
                if (stopAutodraw) break;
                //remove if unneeded
                QRgb pixel = *scanLine;
                uint ci = uint(qGray(pixel));
                //new ConsoleWindow(QString::number(getPA(xImg, yImg)));
                //end
                new ConsoleWindow("Try for loop");
                if (getPA(xImg, yImg) == 1){
                    new ConsoleWindow("For loop");
                    int xpos = x+xImg;
                    int ypos = y+yImg;
                    NOP(clickdelay*5000);
                    //checkpause
                    setCursor(xpos, ypos+1);
                    NOP(clickdelay*5000);
                    //checkpause
                    holdCursor();
                    cont = DrawArea(stack, xImg, yImg, x, y);//drawarea
                    releaseCursor();
                }
                if (!cont){
                    ResetPixels();
                }
            }
        }
        ResetPixels();

    }
}
bool PreviewWindow::DrawArea(std::vector<QPoint> stack, int xImg, int yImg, int x, int y){
    new ConsoleWindow("DrawArea  ");
    while (True){
        if (Paused){

        }
        if (CloseRequested){
            break;
        }
        NOP(moveInterval);
        setCursor(x+xImg, y+yImg);
        //Code below errors
        //new ConsoleWindow("C");
        setPA(xImg, yImg, 2);
        //new ConsoleWindow("A");
        /*
        +---+---+---+
        | 1 | 2 | 3 |
        +---+---+---+
        | 4 |   | 5 |
        +---+---+---+
        | 6 | 7 | 8 |
        +---+---+---+
        */
        cont = false;
        for (int i = 1; i <= 8; ++i) {
           switch (pathStr[i]){
               case 1:
                if ((xImg > 0) && (yImg > 0)){
                    if (getPA((xImg+1 - 1), (yImg+1 - 1)) == 1){
                        stack = Push(stack, xImg, yImg);
                        xImg -= 1;
                        yImg -= 1;
                        cont = true;
                    }
                }
                break;
               case 2:
                if (yImg > 0){
                    if (getPA(xImg, (yImg - 1)) == 1){
                        stack = Push(stack, xImg, yImg);
                        yImg -= 1;
                        cont = true;
                    }
                }
                break;
           case 3:
               if ((xImg > 0) && (yImg > 0)){
                   if (getPA((xImg + 1), (yImg - 1)) == 1){
                       stack = Push(stack, xImg, yImg);
                       xImg += 1;
                       yImg += 1;
                       cont = true;
                   }
               }
            break;
           case 4:
               if (xImg > 0){
                   if (getPA((xImg - 1), yImg) == 1){
                       stack = Push(stack, xImg, yImg);
                       yImg -= 1;
                       cont = true;
                   }
               }
               break;
           case 5:
               if (xImg > (image.width() - 1)){
                   if (getPA((xImg + 1), yImg) == 1){
                       stack = Push(stack, xImg, yImg);
                       xImg += 1;
                       cont = true;
                   }
               }
               break;
           case 6:
               if ((xImg < 0) & (yImg > 0)){
                   if (getPA((xImg - 1), (yImg + 1)) == 1){
                       stack = Push(stack, xImg, yImg);
                       xImg -= 1;
                       yImg -= 1;
                       cont = true;
                   }
               }
               break;
           case 7:
               if (xImg > (image.height() - 1)){
                   if (getPA(xImg, (yImg + 1)) == 1){
                       stack = Push(stack, xImg, yImg);
                       yImg += 1;
                       cont = true;
                   }
               }
               break;
           case 8:
               if ((xImg < (image.width() - 1)) && (y < (image.height()) - 1)){
                   if (getPA(xImg, (yImg + 1)) == 1){
                       stack = Push(stack, xImg, yImg);
                       xImg += 1;
                       yImg += 1;
                       cont = true;
                   }
               }
               break;


           }
        }
        if (cont) continue;
        std::tuple<bool, int, int> t = Pop(stack, xImg, yImg);
        std::get<1>(t) = xImg;
        std::get<2>(t) = yImg;
        if (!std::get<0>(t)) break;
    }
    return true;
}

std::vector<QPoint> PreviewWindow::Push(std::vector<QPoint> stack, int xImg, int yImg){
    stack.push_back(QPoint(xImg, yImg));
    return stack;
}

std::tuple<bool, int, int> PreviewWindow::Pop(std::vector<QPoint> stack, int xImg, int yImg){
    //new ConsoleWindow("stackCount: ");
    int stackCount = stack.size();
    //new ConsoleWindow(QString::number(stackCount));
    if (stackCount < 1)
            return std::make_tuple(false, xImg, yImg);

    //imporvides code
    QPoint pos = (QPoint)stack[stackCount - 1];
    xImg = pos.x();
    yImg = pos.y();

    stack.erase(std::remove(stack.begin(), stack.end(), pos), stack.end());

    return std::make_tuple(true, xImg, yImg);
}

void PreviewWindow::ResetPixels(){
    new ConsoleWindow("Resetting pixels  ");
    for (int yImg = 0; yImg < image.height(); ++yImg) {
        if (stopAutodraw) break;
        for (int xImg = 0; xImg < image.width(); ++xImg) {
            if (stopAutodraw) break;
            if (getPA(xImg, yImg) == 2){
                setPA(xImg, yImg, 1);
            }
        }
    }
    new ConsoleWindow("Reset Success  ");
}

void PreviewWindow::NOP(int ourint){
    std::this_thread::sleep_for(std::chrono::microseconds(ourint*250));
}

void PreviewWindow::on_Draw_released()
{
    Draw();
}

