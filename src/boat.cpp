/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2010 - Christophe Thomas aka Oxygen77

http://qtvlm.sf.net

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#include <QDebug>

#include "Util.h"

#include "MainWindow.h"
#include "StatusBar.h"
#include "orthoSegment.h"
#include "vlmLine.h"
#include "Polar.h"
#include "settings.h"
#include "Player.h"
#include "GshhsReader.h"
#include "boat.h"
#include "boatVLM.h"
#include "Terrain.h"
#include "Orthodromie.h"
#include "DialogChooseMultiple_ctrl.h"
#include "BarrierSet.h"
#include "Projection.h"
#include <QGestureEvent>
#ifndef QT_V5
#include <QTapAndHoldGesture>
#endif
boat::boat(QString      pseudo, bool activated,
           Projection * proj,MainWindow * main,myCentralWidget * parent):
   boatType (BOAT_NOBOAT),
   prevVac (0),
   nextVac (0),
   minSpeedForEngine (0),
   speedWithEngine (4)
{
    this->setParent(parent);
    this->mainWindow=main;
    this->parent=parent;
    squareSize=QSize(8,8);

    polar_list = main->getPolarList();
    connect(this,SIGNAL(getPolar(QString)),polar_list,SLOT(getPolar(QString)));
    connect(this,SIGNAL(showMessage(QString,int)),mainWindow->get_statusBar(),SLOT(showMessage(QString,int)));

    this->pseudo=pseudo;
    selected = false;
    polarName="";
    polarData=NULL;
    changeLocked=false;
    forceEstime=false;
    width=height=0;

    speed=heading=0;
    avg=dnm=loch=ortho=loxo=vmg=0;
    windDir=windSpeed=TWA=0;

    lat=lon=0;
    WP=QPointF(0,0);

    setZValue(Z_VALUE_BOAT);
    setFont(QFont(QApplication::font()));
    setData(0,BOAT_WTYPE);

    fgcolor = QColor(0,0,0);
    int gr = 255;
    bgcolor = QColor(gr,gr,gr,150);
    windEstimeSpeed=-1;
    windEstimeDir=-1;

    estimeLine = new vlmLine(proj,parent->getScene(),Z_VALUE_ESTIME);
    estimeLine->setRoundedEnd(true);
    estimeTimer=new QTimer(this);
    estimeTimer->setInterval(1000);
    connect(estimeTimer,SIGNAL(timeout()),this,SLOT(slot_estimeFlashing()));
    WPLine = new orthoSegment(proj,parent->getScene(),Z_VALUE_ESTIME);
    this->declinaison=0;

    connect(parent, SIGNAL(showALL(bool)),this,SLOT(slot_shSall()));
    connect(parent, SIGNAL(hideALL(bool)),this,SLOT(slot_shHall()));

    trace_drawing = new vlmLine(proj,parent->getScene(),Z_VALUE_BOAT);
    connect(parent,SIGNAL(startReplay(bool)),trace_drawing,SLOT(slot_startReplay(bool)));
    connect(parent,SIGNAL(replay(int)),trace_drawing,SLOT(slot_replay(int)));

    this->proj = proj;
    this->labelHidden=parent->get_shLab_st();

    //qWarning() << "Boat (" << pseudo << ") created labelHidden=" << labelHidden;

    createPopUpMenu();

    //connect(this,SIGNAL(boatUpdated(boat*,bool,bool)),main,SIGNAL(updateRoute(boat *)));
    connect(parent, SIGNAL(shLab(bool)),this,SLOT(slot_shLab(bool)));


    this->activated=false;
    slot_paramChanged();
    this->country="";
    this->drawFlag=false;
    this->flag=QImage();
    flagBad=false;
    updateBoatData();
    this->activated=activated;
    hide();
    this->WPLine->setParent(this);
    QPen penLine2(QColor(Qt::black),1,Qt::DotLine);
    penLine2.setWidthF(1.2);
    WPLine->setLinePen(penLine2);
    estimeTimer->stop();
    estimeLine->slot_showMe();
    estimeLine->setHidden(false);
    WPLine->hideSegment();
    this->stopAndGo="0";

    /*if(activated)
        show();
*/

    connect(this,SIGNAL(boatSelected(boat*)),main,SLOT(slotSelectBoat(boat*)));
    connect(this,SIGNAL(boatUpdated(boat*,bool,bool)),main,SLOT(slotBoatUpdated(boat*,bool,bool)));
    connect(this,SIGNAL(boatLockStatusChanged(boat*,bool)),
            main,SLOT(slotBoatLockStatusChanged(boat*,bool)));

    connect(main,SIGNAL(paramVLMChanged()),this,SLOT(slot_paramChanged()));


    connect(this,SIGNAL(releasePolar(QString)),main,SLOT(releasePolar(QString)));
    connect(parent,SIGNAL(shTrace(bool)),this,SLOT(slot_shTrace(bool)));
    if(Settings::getSetting(enable_Gesture).toString()=="1")
    {
        this->grabGesture(Qt::TapAndHoldGesture);
        this->grabGesture(Qt::TapGesture);
    }
    this->setFlag(QGraphicsWidget::ItemIsSelectable,true);
}

boat::~boat()
{
    setSelected(false);
    disconnect();
    if(!parent->getAboutToQuit())
    {
        if(polarData)
            emit releasePolar(polarData->getName());
        if(estimeLine)
            delete estimeLine;
        if(WPLine)
            delete WPLine;
        if(this->popup)
            delete popup;
        if(this->trace_drawing)
            delete trace_drawing;
        popup=NULL;
        estimeLine=NULL;
        trace_drawing=NULL;
        WPLine=NULL;
    }
}

void boat::createPopUpMenu(void)
{
    popup = new QMenu(parent);
    Util::setFontObject(popup);
    connect(this->popup,SIGNAL(aboutToShow()),parent,SLOT(slot_resetGestures()));
    connect(this->popup,SIGNAL(aboutToHide()),parent,SLOT(slot_resetGestures()));

    ac_select = new QAction("Selectionner",popup);
    popup->addAction(ac_select);
    connect(ac_select,SIGNAL(triggered()),this,SLOT(slot_selectBoat()));

    ac_centerOnboat = new QAction(tr("Center on boat"),popup);
    popup->addAction(ac_centerOnboat);
    connect(ac_centerOnboat,SIGNAL(triggered()),this,SLOT(slot_centerOnBoat()));

    popup->addSeparator();

    ac_estime = new QAction("Afficher estime",popup);
    popup->addAction(ac_estime);
    connect(ac_estime,SIGNAL(triggered()),this,SLOT(slot_toggleEstime()));

    ac_compassLine = new QAction(tr("Tirer un cap"),popup);
    popup->addAction(ac_compassLine);
    connect(ac_compassLine,SIGNAL(triggered()),this,SLOT(slotCompassLine()));
    connect (this,SIGNAL(compassLine(double,double)),mainWindow,SLOT(slotCompassLineForced(double,double)));

    ac_twaLine = new QAction(tr("Tracer une estime TWA"),popup);
    popup->addAction(ac_twaLine);
    connect(ac_twaLine,SIGNAL(triggered()),this,SLOT(slotTwaLine()));

    popup->addSeparator();

    ac_chooseBarrierSet = new QAction(tr("Activate barrier sets"),popup);
    popup->addAction(ac_chooseBarrierSet);
    connect(ac_chooseBarrierSet,SIGNAL(triggered()),this,SLOT(slot_chooseBarrierSet()));
}

void boat::slot_paramChanged()
{
    myColor = QColor(Settings::getSetting(qtBoatColor).toString());
    selColor = QColor(Settings::getSetting(qtBoatSelColor).toString());
    estime_type=Settings::getSetting(estimeType).toInt();
    switch(estime_type)
    {
        case 0: /* time */
            estime_param = Settings::getSetting(estimeTime).toInt();
            break;
        case 1: /* nb vac */
            estime_param = Settings::getSetting(estimeVac).toInt();
            break;
        default: /* dist */
            estime_param = Settings::getSetting(estimeLen).toInt();
            break;
    }
    this->updateBoatString();
    if(activated)
    {
        drawEstime();
    }
    update();
}

/**************************/
/* Select / unselect      */
/**************************/

void boat::slot_selectBoat()
{
    if(this->getplayerName()!=parent->getPlayer()->getName())
    {
        selected=false;
        return;
    }
    selected = true;
    if(Settings::getSetting(hideTrace).toInt()==0)
        trace_drawing->setHidden(false);
    else
        trace_drawing->setHidden(true);
    drawEstime();
    updatePosition(false);
    if(boatType==BOAT_REAL) return;
    updateTraceColor();
    cleanBarrierList();
    emit boatSelected(this);
}

void boat::unSelectBoat(bool needUpdate)
{
    selected = false;
    WPLine->hideSegment();
    if(needUpdate)
    {
        drawEstime();
        update();
        updateTraceColor();
    }
    if(boatType==BOAT_REAL)
        this->stopRead();
}

void boat::slot_centerOnBoat(void) {
    proj->setCenterInMap(getLon(),getLat());
}

/**************************/
/* data access            */
/**************************/

double boat::getBvmgUp(double ws)
{
    if(polarData) return(polarData->getBvmgUp(ws,false));
    return -1;
}
double boat::getBvmgDown(double ws)
{
    if(polarData) return(polarData->getBvmgDown(ws,false));
    return -1;
}

/**************************/
/* Paint & graphics       */
/**************************/

void boat::slot_toggleEstime()
{
    forceEstime=!forceEstime;
    drawEstime();
    update();
}

void boat::paint(QPainter * pnt, const QStyleOptionGraphicsItem * , QWidget * )
{
    qWarning()<<"boat inside paint"<<this->my_str<<QDateTime::currentMSecsSinceEpoch();
    QPen pen(selected?Qt::darkRed:Qt::darkBlue);
    pnt->setFont(QApplication::font());
    if(!labelHidden)
    {
        if(Settings::getSetting(showFlag).toInt()==1 && this->get_boatType()==BOAT_VLM && !flagBad)
        {
            if(flag.isNull())
            {
                if(flag.load(appFolder.value("flags")+this->country+".png"))
                {
                    flag=flag.scaled(30,20,Qt::KeepAspectRatio);
                    drawFlag=true;
                    squareSize=QSize(30,20);
                }
                else { // can't load flag => get it
                    ((boatVLM*)(this))->get_flag();
                    drawFlag=false;
                    squareSize=QSize(8,8);
                }
            }
            else
            {
                drawFlag=true;
                squareSize=QSize(30,20);
            }
        }
        else
        {
            drawFlag=false;
            squareSize=QSize(8,8);
        }
        if(this->get_boatType()==BOAT_VLM)
        {
            if(this->stopAndGo!="0")
                bgcolor=QColor(239,48,36,150);
            else
                bgcolor = QColor(255,255,255,150);
        }
        if (this->isSelected())
        {
            QPen ps(Qt::red);
            ps.setWidthF(3.0);
            pnt->setBrush(Qt::NoBrush);
            pnt->setPen(ps);
            int fingerSize=Util::getFingerSize();
            pnt->drawEllipse(QPoint(0,0),fingerSize,fingerSize);
            pnt->setPen(Qt::NoPen);
        }
        pnt->setBrush(QBrush(bgcolor));
        QPoint labelPos(squareSize.width()/2.0+2,-height/2.0);
        QRect R=QRect(labelPos, QSize(width, height));
        pnt->drawRoundRect(R, 50,50);
        pnt->setBrush(Qt::NoBrush);
        pnt->setPen(pen);
        pnt->drawText(R,Qt::AlignVCenter|Qt::AlignHCenter,my_str);
    }
    pen.setColor(selected?selColor:myColor);
    pen.setWidth(4);
    pnt->setPen(pen);
    if(!drawFlag)
        pnt->fillRect(-squareSize.width()/2,-squareSize.height()/2,squareSize.width(),squareSize.height(),QBrush(selected?selColor:myColor));
    else
        pnt->drawImage(-squareSize.width()/2,-squareSize.height()/2,flag);
    int g = 60;
    pen = QPen(QColor(g,g,g));
    pen.setWidth(1);
    pnt->setPen(pen);
    QPoint labelPos(squareSize.width()/2.0+2,-height/2.0);
    QRect R=QRect(labelPos, QSize(width, height));
    if(!labelHidden)
        pnt->drawRoundRect(R, 50,50);
    //pnt->drawPath(shape());
    //pnt->drawRect(boundingRect());
}

void boat::updateTraceColor(void)
{
    if(selected)
    {
        QPen penLine(selColor,1);
        trace_drawing->setLinePen(penLine);
        trace_drawing->setPointMode(selColor);
    }
    else
    {
        QPen penLine(myColor,1);
        penLine.setWidthF(0.8);
        trace_drawing->setLinePen(penLine);
        trace_drawing->setPointMode(myColor);
    }
    trace_drawing->setNbVacPerHour(3600/getVacLen());
    if(Settings::getSetting(hideTrace).toInt()==0)
    {
        trace_drawing->slot_showMe();
        trace_drawing->setHidden(false);
    }
    else
        trace_drawing->setHidden(true);
}

void boat::drawEstime(void)
{
    bool getStartEstimeSpeedFromGrib = Settings::getSetting(startSpeedEstime).toInt()==1;
    DataManager * dataManager = parent->get_dataManager();
    if( getStartEstimeSpeedFromGrib && dataManager && dataManager->isOk() && this->getPolarData())
    {
        double wind_speed=0;
        double wind_angle=0;
        double current_speed=-1;
        double current_angle=0;
        if(dataManager->getInterpolatedWind(lon,lat,this->getPrevVac()+this->getVacLen(),&wind_speed,&wind_angle) &&
                !(this->get_boatType()==BOAT_REAL && getSpeed()==0 && getHeading()==0))
        {
            wind_angle=radToDeg(wind_angle);
            if(dataManager->getInterpolatedCurrent(lon,lat,this->getPrevVac()+this->getVacLen(),&current_speed,&current_angle))
            {
                current_angle=radToDeg(current_angle);
                QPointF p=Util::calculateSumVect(wind_angle,wind_speed,current_angle,current_speed);
                wind_speed=p.x();
                wind_angle=p.y();
            }
            double twa=getHeading()-wind_angle;
            if(twa>=360) twa=twa-360;
            if(twa<0) twa=twa+360;
            if(qAbs(twa)>180)
            {
                if(twa<0)
                    twa=360+twa;
                else
                    twa=twa-360;
            }
            double newSpeed=getPolarData()->getSpeed(wind_speed,twa);
            double cap=getHeading();
            if(current_speed>0)
            {
                QPointF p=Util::calculateSumVect(cap,newSpeed,AngleUtil::A360(current_angle+180.0),current_speed);
                newSpeed=p.x(); //in this case newSpeed is SOG
                cap=p.y(); //in this case cap is COG
            }
            drawEstime(cap, newSpeed);
        }
    }
    else
        drawEstime(getHeading(),getSpeed());
}

void boat::drawEstime(double myHeading, double mySpeed)
{

    estimeLine->deleteAll();
    /*should we draw something?*/
    if(isUpdating() || !getStatus())
        return;
    estimeLine->setHidden(false);
    QPen penLine1(QColor(Settings::getSetting(estimeLineColor).value<QColor>()),1,Qt::SolidLine);
    penLine1.setWidthF(Settings::getSetting(estimeLineWidth).toDouble());

    /* draw estime */
    if(getIsSelected() || getForceEstime())
    {
        double tmp_lat,tmp_lon;
        double estime;

        switch(estime_type)
        {
            case 0: /* time */
                if(mySpeed<0.001)
                    estime=0;
                else
                    estime = (double)estime_param/60.0*(double)mySpeed;
                break;
            case 1: /* nb vac */
            {
                const double estime_param_2=getVacLen();
                if(mySpeed<0.001)
                    estime=0;
                else
                    estime = ((double) estime_param*estime_param_2)*mySpeed/3600.0;
                break;
            }
            default: /* dist */
                estime = estime_param;
                break;
        }


        Util::getCoordFromDistanceLoxo(lat,lon,estime,myHeading,&tmp_lat,&tmp_lon);
        time_t estime_time=0;
        DataManager * dataManager=parent->get_dataManager();
        if(mySpeed>0.001 && dataManager && dataManager->isOk())
        {
            estime_time=(estime/mySpeed)*3600;
            dataManager->getInterpolatedWind(lon,lat,this->getPrevVac()+this->getVacLen()+estime_time,&windEstimeSpeed,&windEstimeDir);
            windEstimeDir=radToDeg(windEstimeDir);
        }
        else
            windEstimeSpeed=-1;
        GshhsReader *map=parent->get_gshhsReader();
        double I1,J1,I2,J2;
        proj->map2screenDouble(lon,lat,&I1,&J1);
        proj->map2screenDouble(tmp_lon,tmp_lat,&I2,&J2);
        bool coastDetected=false;
        if(map && map->getQuality()>=2)
        {
            //qWarning("crossing (%.5f,%.5f,%.5f,%.5f) (%.5f,%.5f,%.5f,%.5f)",I1,J1,I2,J2,lon,lat,tmp_lon,tmp_lat);
            //qWarning("estime=%.5f, myHeading=%.5f",estime,myHeading);
            if(estime>0.0001 && map->crossing(QLineF(I1,J1,I2,J2),QLineF(lon,lat,tmp_lon,tmp_lat)))
            {
                estimeTimer->start();
                penLine1.setColor(Qt::red);
                coastDetected=true;
            }
        }
        if(!coastDetected)
        {
            estimeTimer->stop();
            if(this->get_boatType()==BOAT_VLM)
            {
                for(int n=0;n<this->getGates().count();++n)
                {
                    if(this->getGates().at(n)->getHidden()) continue;
                    double LonTmp=getGates().at(n)->getPoints()->first().lon;
                    double LatTmp=getGates().at(n)->getPoints()->first().lat;
                    double Gx1,Gy1,Gx2,Gy2;
                    proj->map2screenDouble(LonTmp,LatTmp,&Gx1,&Gy1);
                    LonTmp=getGates().at(n)->getPoints()->last().lon;
                    LatTmp=getGates().at(n)->getPoints()->last().lat;
                    proj->map2screenDouble(LonTmp,LatTmp,&Gx2,&Gy2);
                    if(estime>0.0001 && my_intersects(QLineF(I1,J1,I2,J2),QLineF(Gx1,Gy1,Gx2,Gy2)))
                    {
                        penLine1.setColor(Qt::darkGreen);
                        penLine1.setWidthF(penLine1.widthF()*1.5);
                    }
                    break;
                }
            }
        }
        estimeLine->setLinePen(penLine1);
        estimeLine->addPoint(lat,lon);
        estimeLine->addPoint(tmp_lat,tmp_lon);
        estimeLine->slot_showMe();
        //estimeLine->initSegment(i1,j1,i2,j2);
        /* draw ortho to wp */
//        if(WP.x() != 0 && WP.y() != 0)
//        {
//            WPLine->setLinePen(penLine2);
//            qWarning()<<"iniSegment(2)";
//            WPLine->initSegment(lon,lat,WP.x(),WP.y());
//        }
        this->updateHint();
    }
}
#  define INTER_MAX_LIMIT 1.0000001
#  define INTER_MIN_LIMIT -0.0000001
bool boat::my_intersects(QLineF line1,QLineF line2) const
{

    // implementation is based on Graphics Gems III's "Faster Line Segment Intersection"
    const QPointF a = line1.p2() - line1.p1();
    const QPointF b = line2.p1() - line2.p2();
    const QPointF c = line1.p1() - line2.p1();

    const qreal denominator = a.y() * b.x() - a.x() * b.y();
    if (denominator == 0)
        return false;

    const qreal reciprocal = 1 / denominator;
    const qreal na = (b.y() * c.x() - b.x() * c.y()) * reciprocal;

    if (na < INTER_MIN_LIMIT || na > INTER_MAX_LIMIT)
        return false;

    const qreal nb = (a.x() * c.y() - a.y() * c.x()) * reciprocal;
    if (nb < INTER_MIN_LIMIT || nb > INTER_MAX_LIMIT)
        return false;

    return true;
}

/**************************/
/* shape & boundingRect   */
/**************************/
void boat::slot_estimeFlashing()
{
    if(!selected)
    {
        estimeTimer->stop();
        estimeLine->setHidden(true);
        WPLine->hide();
        return;
    }
    estimeLine->setHidden(!estimeLine->getHidden());
}

void boat::slotCompassLine()
{
    ac_compassLine->setDisabled(true);
    double i1,j1;
    proj->map2screenDouble(this->lon,this->lat,&i1,&j1);
    emit compassLine(i1,j1);
}
QPainterPath boat::shape() const
{
    QPainterPath path;
    int sh=Util::getFingerSize()+4;
    QRectF R1=QRectF(-sh,-sh,sh*2,sh*2);
    if(isSelected())
        path.addEllipse(R1);
    else
#ifdef __ANDROID__
    {
        R1=QRectF(-sh/2,-sh/2,sh,sh);
        path.addEllipse(R1);
    }
#else
        path.addRect(-squareSize.width()/2.0-1,-squareSize.height()/2.0-1,squareSize.width()+2,squareSize.height()+2);
#endif
    path.addRect(squareSize.width()/2.0+2,-height/2.0-2,width+4,height+4);
    path.setFillRule(Qt::WindingFill);
    return path;
}

QRectF boat::boundingRect() const
{
    QRectF R1(squareSize.width()/2.0+2,-height/2.0-2,width+4,height+4); //label
    QRectF R2=R1;
    if(isSelected())
    {
        int sh=Util::getFingerSize()+4;
        R2=QRectF(-sh,-sh,sh*2,sh*2);
    }
    QRectF R3(-squareSize.width()/2.0-1,-squareSize.height()/2.0-1,squareSize.width()+2,squareSize.height()+2);
    return R1.united(R2).united(R3);
}

/**************************/
/* Data update            */
/**************************/

void boat::updateBoatData()
{
    if(!activated)
        return;
    if(this->get_boatType()==BOAT_VLM && !((boatVLM*)this)->isInitialized())
    {
        //qWarning()<<"boat not initialized, skipping updateBoatData()";
        return;
    }
    updateBoatString();
    reloadPolar();
    updatePosition(false);
    updateHint();
}
void boat::drawOnMagnifier(Projection * mProj, QPainter * pnt)
{
    double I1,J1;
    mProj->map2screenDouble(lon,lat,&I1,&J1);
    int boat_i,boat_j;
    boat_i=qRound(I1)-3;
    boat_j=qRound(J1)-(height/2);
    int dy = height/2;
    QPen pen(selected?Qt::darkRed:Qt::darkBlue);
    pnt->setFont(QApplication::font());
    if(!labelHidden)
    {
        if(Settings::getSetting(showFlag).toInt()==1 && this->get_boatType()==BOAT_VLM)
        {
            if(flag.isNull())
            {
                if(flag.load(appFolder.value("flags")+this->country+".png"))
                {
                    flag=flag.scaled(30,20,Qt::KeepAspectRatio);
                    drawFlag=true;
                }
                else
                    drawFlag=false;
            }
            else
            {
                drawFlag=true;
            }
        }
        else
        {
            drawFlag=false;
        }
        if(this->get_boatType()==BOAT_VLM)
        {
            if(this->stopAndGo!="0")
                bgcolor=QColor(239,48,36,150);
            else
                bgcolor = QColor(255,255,255,150);
        }
        QColor bgcolor_m=bgcolor;
        bgcolor_m.setAlpha(255);
        pnt->setBrush(QBrush(bgcolor_m));
        if(!drawFlag)
            pnt->drawRoundRect(boat_i+9,boat_j+0, width,height, 50,50);
        else
            pnt->drawRoundRect(boat_i+21,boat_j+0, width,height, 50,50);
        pnt->setBrush(Qt::NoBrush);
        pnt->setPen(pen);
        QFontMetrics fm(QApplication::font());
        int w = fm.width("_")+1;
        if(!drawFlag)
            pnt->drawText(boat_i+w+9,boat_j+height-height/4,my_str);
        else
            pnt->drawText(boat_i+w+21,boat_j+height-height/4,my_str);
    }
    QColor selColor_m=selColor;
    selColor_m.setAlpha(255);
    QColor myColor_m=myColor;
    myColor_m.setAlpha(255);
    pen.setColor(selected?selColor_m:myColor_m);
    pen.setWidth(4);
    pnt->setPen(pen);
    if(!drawFlag)
        pnt->fillRect(boat_i+0,boat_j+dy-3,7,7, QBrush(selected?selColor_m:myColor_m));
    else
        pnt->drawImage(boat_i+-11,boat_j+dy-9,flag);
    int g = 60;
    pen = QPen(QColor(g,g,g));
    pen.setWidth(1);
    pnt->setPen(pen);
    if(!labelHidden)
    {
        if(!drawFlag)
            pnt->drawRoundRect(boat_i+9,boat_j+0, width,height, 50,50);
        else
            pnt->drawRoundRect(boat_i+21,boat_j+0, width,height, 50,50);
    }

    //drawEstime();
}

void boat::updatePosition(const bool &fromZoom)
{
    double I1,J1;
    proj->map2screenDouble(lon,lat,&I1,&J1);
    setPos(I1, J1);
    //qWarning()<<"updatePosition with"<<fromZoom<<selected<<WP.x()<<WP.y();
    if(!fromZoom)
    {
        drawEstime();
        if(selected && WP.x() != 0 && WP.y() != 0)
        {
            WPLine->showSegment();
            //qWarning()<<"iniSegment(1)";
            WPLine->initSegment(lon,lat,WP.x(),WP.y());
        }
        else
            WPLine->hideSegment();
    }
}

double boat::getWPdir(void)
{
    double dirAngle;
    if(WP.x() != 0 && WP.y() != 0)
    {
        Orthodromie orth = Orthodromie(QPointF(lon,lat),WP);
        dirAngle = orth.getAzimutDeg();
    }
    else
        dirAngle=-1;
    return dirAngle;
}

void boat::setWP(QPointF ,double ) {
    qWarning() << "setWp call => from boat!!! not doing anything";
}

void boat::slot_projectionUpdated()
{
    if(activated)
        updatePosition(true);
}

void boat::setStatus(bool activated)
{
     //qWarning() << "BOAT: " << this->getBoatPseudo() << " setStatus=" << activated;
     this->activated=activated;
     setVisible(activated);


     if(activated)
     {
         //qWarning()<<"before slot_paramChanged";
         slot_paramChanged();
         //qWarning()<<"before updateBoatData";
         updateBoatData();
         //qWarning()<<"after updateBoatData";
     }
     //qWarning()<<"activated="<<activated;

     if(!activated)
     {
        WPLine->hideSegment();
        estimeTimer->stop();
        estimeLine->setHidden(true);;
        if(boatType==BOAT_REAL)
            this->stopRead();
     }
     else
     {
         estimeLine->setHidden(false);
         WPLine->showSegment();
     }
}

void boat::playerDeActivated(void)
{
    //qWarning() << "Deactivation of: " << this->getBoatPseudo();
    if(selected)
        this->unSelectBoat(false);
    this->hide();
    setVisible(false);
    trace_drawing->setHidden(true);
    WPLine->hideSegment();
    estimeTimer->stop();
    estimeLine->setHidden(true);;
    if(boatType==BOAT_REAL)
        this->stopRead();
}

void boat::setParam(QString pseudo)
{
     this->pseudo=pseudo;
}

void boat::setParam(QString pseudo, bool activated)
{
    setStatus(activated);
    setParam(pseudo);
}

void boat::reloadPolar(bool forced)
{
    //qWarning()<<"inside reloadPolar with forced=" << forced << " for " << this->getBoatPseudo();
    if(forced && polarName.isEmpty() && polarData)
        polarName=polarData->getName();
    //qWarning()<<"polarName="<<polarName;
    if(polarName.isEmpty()) /* nom de la polaire est vide => pas de chargement */
    {
       //qWarning() << "Polar name empty => nothing to load";
        if(polarData!=NULL) /* doit on faire un release tt de meme? */
        {
            emit releasePolar(polarData->getName());
            polarData=NULL;
        }
        return;
    }

    if(!forced && polarData != NULL && polarName == polarData->getName()) /* reload inutile */
    {
        //qWarning() << "Polar already loaded => nothing to do"<<forced;
        return;
    }

    if(polarData!=NULL)
    {
        //qWarning() << "Releasing polar " << polarData->getName();
        emit releasePolar(polarData->getName()); /* release si une polaire deja chargee */
    }
    connect(polar_list,SIGNAL(polarLoaded(QString,Polar *)),this,SLOT(polarLoaded(QString,Polar *)));
    //qWarning() << pseudo << " request polar " << polarName;
    emit getPolar(polarName);
}

void boat::polarLoaded(QString polarName,Polar * polarData) {
    disconnect(polar_list,SIGNAL(polarLoaded(QString,Polar *)),this,SLOT(polarLoaded(QString,Polar *)));
    //qWarning() << pseudo << " get polar returned: " << polarName;
    if(this->polarName==polarName)
    {
        this->polarData=polarData;
        /*if(polarData)
            qWarning() << polarData->getName() << " received";
        else
            qWarning() << "void polar received";*/
    }
}

void boat::setLockStatus(bool status)
{
//n'a probablement d interet que pour les boat VLM
    if(status!=changeLocked)
    {
        changeLocked=status;
        emit boatLockStatusChanged(this,status);
    }
}

void boat::slot_updateGraphicsParameters()
{
    if(activated)
    {
        drawEstime();
        showNextGates();
    }
}

/********************************************************
 *  Barrier
 *******************************************************/

void boat::add_barrierSet(BarrierSet* set) {
    if(set && !barrierSets.contains(set)) {
        barrierSets.append(set);
        updateBarrierKeys();
    }

    if(selected) set->set_isHidden(false);
}

void boat::rm_barrierSet(BarrierSet* set) {
    if(set && barrierSets.contains(set)) {
        barrierSets.removeAll(set);
        updateBarrierKeys();
    }
    if(selected) set->set_isHidden(true);
}

void boat::update_barrierKey(BarrierSet* set) {
    if(set && barrierSets.contains(set)) {
        updateBarrierKeys();
    }
}

void boat::slot_chooseBarrierSet(void) {
    cleanBarrierList();
    DialogChooseMultiple_ctrl::chooseMultipleBarrierSet(mainWindow->getMy_centralWidget() ,&barrierSets,this);
    updateBarrierKeys();
}

void boat::updateBarrierKeys(void) {
    barrierKeys.clear();
    for(int i=0;i<barrierSets.count();i++)
        barrierKeys.append(barrierSets.at(i)->get_key());
}

void boat::clear_barrierSet(void) {
    barrierSets.clear();
    if(selected) BarrierSet::releaseState();
}

void boat::cleanBarrierList(void) {
    BarrierSet::get_barrierSetListFromKeys(barrierKeys,&barrierSets);

    if(selected) {
        BarrierSet::releaseState();
        for(int i=0;i<barrierSets.count();++i)
            barrierSets.at(i)->set_isHidden(false);
    }

    /*qWarning() << "nb keys: " << barrierKeys.count();
    qWarning() << "key list: \n" << barrierKeys;
    qWarning() << "nb in barrierSets: " <<barrierSets.count();
    barrierSets.at(0)->printSet();*/
}

bool boat::cross(QLineF line) {
    QList<BarrierSet*>::const_iterator i;
    for(i=barrierSets.constBegin();i<barrierSets.constEnd();++i)
        if((*i)->cross(line))
            return true;
    return false;
}

/**************************/
/* Events                 */
/**************************/

QVariant boat::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if(change==ItemToolTipHasChanged)
    {
        if(isSelected())
            parent->showToolTip(toolTip(),false);
    }
    if(change==ItemSelectedHasChanged)
    {
        prepareGeometryChange();
        if(!isSelected())
            parent->showToolTip("");
        else
        {
            parent->clearOtherSelected(this);
            parent->showToolTip(toolTip());
        }
    }
    return QGraphicsWidget::itemChange(change,value);
}
void boat::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    qWarning()<<"mouse press event in boat detected";
    QGraphicsWidget::mousePressEvent(e);
    e->accept();
//    parent->get_terrain()->ungrabGesture(Qt::TapGesture);
//    parent->get_terrain()->ungrabGesture(Qt::TapAndHoldGesture);
    return;
}
void boat::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    QGraphicsWidget::mouseReleaseEvent(e);
//    parent->get_terrain()->grabGesture(Qt::TapGesture);
//    parent->get_terrain()->grabGesture(Qt::TapAndHoldGesture);
    return;
}

void boat::contextMenuEvent(QGraphicsSceneContextMenuEvent * )
{
    qWarning()<<"inside contextMenuEvent (boat)";
    this->showContextualMenu(QCursor::pos().x(),QCursor::pos().y());
}
void boat::showContextualMenu(const int &xPos, const int &yPos)
{
    qWarning()<<"inside showContextualMenu (boat)";
    bool onlyLineOff = false;

    switch(parent->getCompassMode(this->x(),this->y()))
    {
        case 0:
            /* not showing menu line, default text*/
            ac_compassLine->setText(tr("Tirer un cap"));
            ac_compassLine->setEnabled(true);
            break;
        case 1:
            /* showing text for compass line off*/
            ac_compassLine->setText(tr("Arret du cap"));
            ac_compassLine->setEnabled(true);
            onlyLineOff=true;
            break;
        case 2:
        case 3:
            ac_compassLine->setText(tr("Tirer un cap"));
            ac_compassLine->setEnabled(true);
            break;
        }

    if(forceEstime)
        ac_estime->setText(tr("Cacher estime"));
    else
        ac_estime->setText(tr("Afficher estime"));

    if(onlyLineOff)
    {
        ac_select->setEnabled(false);
        ac_estime->setEnabled(false);
    }
    else
    {
        ac_select->setEnabled(!mainWindow->get_selPOI_instruction());
        ac_estime->setEnabled(!selected);
    }
    popup->exec(QPoint(xPos,yPos));
}
bool boat::sceneEvent(QEvent *event)
{
    //qWarning()<<"event detected in BOAT"<<event->type();
    if (event->type() == QEvent::Gesture)
    {
        QGraphicsWidget::sceneEvent(event);
        event->accept();
        QGestureEvent * gestureEvent=static_cast<QGestureEvent*>(event);
        foreach (QGesture * gesture, gestureEvent->gestures())
        {
            gestureEvent->accept(gesture);
            if(gesture->gestureType()==Qt::TapGesture)
                qWarning()<<"tap gesture in boat"<<gesture->state();
            else if (gesture->gestureType()==Qt::TapAndHoldGesture)
            {
                qWarning()<<"tapAndHold gesture in boat"<<gesture->state();
                QTapAndHoldGesture *p=static_cast<QTapAndHoldGesture*>(gesture);
                if(p->state()==Qt::GestureFinished && this->scene()->mouseGrabberItem()==this)
                    this->showContextualMenu(p->position().x(),p->position().y());
            }
            else
                qWarning()<<"unexpected gesture in boat:"<<gesture->gestureType()<<gesture->state();
        }
        return true;
    }
    return QGraphicsWidget::sceneEvent(event);
}
QList<vlmLine*> boat::getGates()
{
    QList<vlmLine*> empty;
    return empty;
}
void boat::slot_shTrace(const bool &b)
{
    this->trace_drawing->setHidden(b);
    if(!b)
        trace_drawing->slot_showMe();
}
