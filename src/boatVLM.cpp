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

#include <QMessageBox>
#include <QDebug>

#include "boatVLM.h"

#include "MainWindow.h"
#include "vlmLine.h"
#include "parser.h"
#include "Util.h"
#include "Polar.h"
#include "settings.h"
#include "orthoSegment.h"


boatVLM::boatVLM(QString pseudo, bool activated, int boatId, int playerId,Player * player, int isOwn,
                 Projection * proj,MainWindow * main, myCentralWidget * parent,
                 inetConnexion * inet): boat(pseudo,activated,proj,main,parent), inetClient(inet)
{    
    this->boat_type=BOAT_VLM;
    setFont(QFont("Helvetica",9));

    this->isOwn=isOwn;
    name="";
    alias="";
    useAlias=false;
    inetClient::setName(pseudo);
    this->player=player;
    boat_id=boatId;
    player_id=playerId;
    race_id=0;
    pilototo.clear();
    hasPilototo=false;
    polarVlm="";
    forcePolar=false;
    updating=false;
    vacLen=300;
    doingSync=false;
    gatesLoaded=false;
    nWP=0;
    this->porteHidden=parent->get_shPor_st();
    firstSynch=false;
    race_name="";
    trace.clear();
    playerName=player->getName();
    needAuth=true;
    this->rank=1;
    connect(parent, SIGNAL(shPor(bool)),this,SLOT(slot_shPor()));

    myCreatePopUpMenu();
    this->initialized=false;
    own=QString();
    //setStatus(activated);
}

boatVLM::~boatVLM(void)
{
    if(gatesLoaded)
    {
        vlmLine * porte;
        QListIterator<vlmLine*> i (gates);
        while(i.hasNext())
        {
            porte=i.next();
            //parent->getScene()->removeItem(porte);
            if(porte)
            {
                if(!parent->getAboutToQuit())
                    delete porte;
            }
        }
    }
    if(!parent->getAboutToQuit())
    {
        if(estimeLine)
            delete this->estimeLine;
        if(trace_drawing)
            delete this->trace_drawing;
        if(this->WPLine)
            delete this->WPLine;
    }
}

void boatVLM::updateData(boatData * data)
{
//    this->boat_name=data->name;
//    this->playerName=data->playerName;
    this->boat_id=data->idu;
    this->isOwn=data->isOwn;
    this->pseudo=data->pseudo;
    this->name=data->name;
    /* Not updating activated status */
    //this->setStatus(data->engaged>0);
}

/***********************************/
/* VLM link                        */


void boatVLM::slot_getData(bool doingSync)
{
    updating=true;
    doRequest(VLM_REQUEST_BOAT);
    this->doingSync=doingSync;
}

void boatVLM::slot_getDataTrue()
{
    slot_getData(true);
}

void boatVLM::doRequest(int requestCmd)
{
    if(!activated)
    {
        while(!gates.isEmpty())
           delete gates.takeFirst();
        this->gatesLoaded=false;
    }
    if(!activated && initialized)
    {
        updating=false;
        return;
    }

    if(hasInet())
    {
        if(hasRequest() )
        {
            qWarning() << "request already running for " << pseudo;
            return;
        }
        else
        {
            /*qWarning() << "Doing request " << requestCmd << " for " << pseudo */ ;
        }

        QString page;

        switch(requestCmd)
        {
            case VLM_REQUEST_BOAT:
                QTextStream(&page) << "/ws/boatinfo.php?forcefmt=json"
                            << "&select_idu="<<this->boat_id;
                break;
            case VLM_REQUEST_TRJ:
            {
                trace_drawing->deleteAll();
                if(race_id==0)
                {
                    qWarning() << "boat Acc no request TRJ for:" << pseudo << " id=" << boat_id;
                    return;
                }
                time_t et=QDateTime::currentDateTime().toUTC().toTime_t();
                time_t st=et-(Settings::getSetting("trace_length",12).toInt()*60*60);
                clearCurrentRequest();
                QTextStream(&page)
                        << "/ws/boatinfo/tracks.php?"
                        << "idu="
                        << boat_id
                        << "&starttime="
                        << st;
                break;
            }
            case VLM_REQUEST_GATE:
                QTextStream(&page) << "/ws/raceinfo.php?idrace="<<race_id;
                break;
             default:
                qWarning() << "[boatVLM-doRequest] error: unknown request: " << requestCmd;
                break;
        }

        inetGet(requestCmd,page);
    }
    else
    {
         qWarning("Creating dummy data");
         boat_id = 0;
         pseudo = "test";
         race_id = 0;
         lat = 0;
         lon = 0;
         vacLen=300;
         race_name = "Test race";
         updating=false;
         updateBoatData();
    }
}

#define printData(JSON,DATA) qWarning() << DATA << JSON[DATA].toString();

void boatVLM::requestFinished (QByteArray res_byte)
{
    double latitude=0,longitude=0;

    QString res(res_byte);

    QWARN << "Request finished: " << getCurrentRequest() << " boat: " << this->boat_id;

    switch(getCurrentRequest())
    {
        case VLM_REQUEST_WS:
            break;

        case VLM_REQUEST_BOAT:
            {
                QJson::Parser parser;
                bool ok;

                QVariantMap result = parser.parse (res_byte, &ok).toMap();
                if (!ok) {
                    qWarning() << "Error parsing json data " << res_byte;
                    qWarning() << "Error: " << parser.errorString() << " (line: " << parser.errorLine() << ")";
                }

                if(1 /*checkWSResult(res_byte,"BoatVLM_boatf",mainWindow)*/) // pas de wsCheck car VLM ne renvoit pas "success"
                {
                    hasPilototo=false;
                    newRace=false;
                    vacLen=300;
                    pilototo.clear();
                    for(int i=0;i<5;i++)
                        pilototo.append("none");


                    if(race_id != result["RAC"].toInt())
                        newRace=true;
                    if(result["error"].toMap()["code"].toString()=="XXXXXXX" || boat_id!=result["IDU"].toInt()) /* cas de la radiation du boatsit*/
                    {
                        /* clearing trace */
                        trace.clear();
                        trace_drawing->deleteAll();
                        /*updating everything*/
                        updateBoatData();
                        updating=false;
                        QMessageBox::warning(0,QObject::tr("Bateau au ponton"),
                                             QObject::tr("Le bateau ")+"'"+pseudo+"' "+QObject::tr("a ete desactive car vous n'etes plus boatsitter"));
                        setStatus(false);
                        while(!gates.isEmpty())
                           delete gates.takeFirst();
                        this->gatesLoaded=false;
                        emit boatUpdated(this,newRace,doingSync);
                        emit hasFinishedUpdating();
                        return;
                    }

                    boat_id     = result["IDU"].toInt();
                    race_id     = result["RAC"].toInt();
                    name        = result["IDB"].toString();
                    own         = result["OWN"].toString();
                    latitude    = result["LAT"].toDouble();
                    longitude   = result["LON"].toDouble();
                    race_name   = result["RAN"].toString();
                    speed       = (float) result["BSP"].toDouble();
                    heading     = (float) result["HDG"].toDouble();
                    avg         = (float) result["AVG"].toDouble();
                    dnm         = (float) result["DNM"].toDouble();
                    loch        = (float) result["LOC"].toDouble();
                    ortho       = (float) result["ORT"].toDouble();
                    loxo        = (float) result["LOX"].toDouble();
                    vmg         = (float) result["VMG"].toDouble();
                    windDir     = (float) result["TWD"].toDouble();
                    country     = result["CNT"].toString();
                    windSpeed   = (float) result["TWS"].toDouble();
                    WPLat       = result["WPLAT"].toDouble();
                    WPLon       = result["WPLON"].toDouble();
                    WPHd        = (float) result["H@WP"].toDouble();
                    pilotType   = result["PIM"].toInt();
                    pilotString = result["PIP"].toString();
                    TWA         = (float) result["TWA"].toDouble();
                    ETA         = result["ETA"].toString();
                    score       = result["POS"].toString();
                    prevVac     = result["LUP"].toUInt();
                    nextVac     = result["NUP"].toUInt();
                    nWP         = result["NWP"].toInt();
                    rank        = result["RNK"].toInt();
                    pilototo[0] = result["PIL1"].toString();
                    pilototo[1] = result["PIL2"].toString();
                    pilototo[2] = result["PIL3"].toString();
                    pilototo[3] = result["PIL4"].toString();
                    pilototo[4] = result["PIL5"].toString();
                    polarVlm = result["POL"].toString();
                    email = result["EML"].toString();
                    vacLen = result["VAC"].toInt();

                    lat = latitude/1000;
                    lon = longitude/1000;
                    initialized=true;
                    if(!activated)
                    {
                        updating=false;
                        emit hasFinishedUpdating();
                        return;
                    }
//                    if(result["RAC"].toInt() == 0)
//                    {
//                        initialized=true;
//                        QMessageBox::warning(0,QObject::tr("Bateau au ponton"),
//                                             QString("Le bateau '")+pseudo+"' a ete desactive car hors course");
//                        setStatus(false);
//                        emit boatUpdated(this,false,doingSync);
//                        emit hasFinishedUpdating();
//                        break;
//                    }
                    hasPilototo=true;
                    if(newRace)
                    {
                        while(!gates.isEmpty())
                            delete gates.takeFirst();
                        this->gatesLoaded=false;
                    }
                    /* request trace points */
                    if(race_id==0)
                    {
                        /* clearing trace */
                        trace.clear();
                        trace_drawing->deleteAll();
                        /*updating everything*/
                        updateBoatData();
                        updating=false;
                        QMessageBox::warning(0,QObject::tr("Bateau au ponton"),
                                             QObject::tr("Le bateau ")+"'"+pseudo+"' "+QObject::tr("a ete desactive car hors course"));
                        setStatus(false);
                        while(!gates.isEmpty())
                           delete gates.takeFirst();
                        this->gatesLoaded=false;
                        emit boatUpdated(this,newRace,doingSync);
                        emit hasFinishedUpdating();
                    }
                    else
                    {
                        doRequest(VLM_REQUEST_TRJ);
                    }
                }
                else
                {
                    /* clearing trace */
                    trace.clear();
                    trace_drawing->deleteAll();
                    updateBoatData();
                    updating=false;
                    setStatus(false);
                    emit boatUpdated(this,newRace,doingSync);
                    emit hasFinishedUpdating();
                }
            }
            break;
        case VLM_REQUEST_TRJ:
            emit getTrace(res_byte,&trace);
            trace_drawing->setPoly(trace);

            /* we can now update everything */
            updateBoatData();
            updateTraceColor();
            //drawEstime();

            if(race_id!=0 && !gatesLoaded)
            {
                updating=false;
                drawEstime();
                updating=true;
                doRequest(VLM_REQUEST_GATE);
            }
            else
            {
                updating=false;
                drawEstime();
                emit hasFinishedUpdating();
                emit boatUpdated(this,newRace,doingSync);
            }
            showNextGates();
            break;
        case VLM_REQUEST_GATE:
            {
                QJson::Parser parser;
                bool ok;

                QVariantMap result = parser.parse (res_byte, &ok).toMap();
                if (!ok) {
                    qWarning() << "Error parsing json data " << res_byte;
                    qWarning() << "Error: " << parser.errorString() << " (line: " << parser.errorLine() << ")";
                }

                //qWarning() << "id Race: " << result["idraces"].toString();
                //qWarning() << "race name: " << result["racename"].toString();

                QVariantMap wps= result["races_waypoints"].toMap();
                for(int i=1;true;i++)
                {
                    QString str;
                    QVariantMap wp= wps[str.setNum(i)].toMap();
                    if(wp.isEmpty()) break;
                    //qWarning()<<"wp: "<<wp["libelle"].toString();
                    double lonPorte1=wp["longitude1"].toDouble()/1000.000;
                    double latPorte1=wp["latitude1"].toDouble()/1000.000;
                    double lonPorte2=wp["longitude2"].toDouble()/1000.000;
                    double latPorte2=wp["latitude2"].toDouble()/1000.000;
                    int wpformat=wp["wpformat"].toInt();
                    bool oneBuoy=false;
                    bool iceGateN=false;
                    bool iceGateS=false;
                    bool clockWise=false;
                    bool antiClockWise=false;
                    bool crossOnce=false;
                    if(wpformat>=1024)
                    {
                        crossOnce=true;
                        wpformat=wpformat-1024;
                    }
                    if(wpformat>=512)
                    {
                        antiClockWise=true;
                        wpformat=wpformat-512;
                    }
                    if(wpformat>=256)
                    {
                        clockWise=true;
                        wpformat=wpformat-256;
                    }
                    if(wpformat>=32)
                    {
                        iceGateS=true;
                        wpformat=wpformat-32;
                    }
                    if(wpformat>=16)
                    {
                        iceGateN=true;
                        wpformat=wpformat-16;
                    }
                    if(wpformat>=1)
                        oneBuoy=true;


                    vlmLine * porte=new vlmLine(proj,parent->getScene(),Z_VALUE_GATE);
                    connect(parent, SIGNAL(shLab(bool)),porte,SLOT(slot_shLab(bool)));
                    porte->setGateMode("WP"+str+": "+wp["libelle"].toString());
                    if(iceGateN)
                        porte->setIceGate(1);
                    else if(iceGateS)
                        porte->setIceGate(2);
                    else
                        porte->setIceGate(0);
                    porte->addPoint(latPorte1,lonPorte1);
                    if((qRound(lonPorte1*1000)==qRound(lonPorte2*1000) && qRound(latPorte1*1000)==qRound(latPorte2*1000))||oneBuoy)
                    {
                        porte->setPorteOnePoint();
                        Util::getCoordFromDistanceAngle(latPorte1, lonPorte1, 500,wp["laisser_au"].toDouble()+180,&latPorte2,&lonPorte2);
                        oneBuoy=true;
                    }
                    porte->addPoint(latPorte2,lonPorte2);
                    QPen penLine(Qt::black,1);
                    penLine.setWidthF(3);
                    porte->setLinePen(penLine);
                    porte->setHidden(true);
                    QString tip;
                    if(iceGateN)
                        tip=tr("Passer au moins une fois au Sud de cette ligne<br>")+
                            tr("Vous n'etes pas oblige de couper la ligne");
                    else if(iceGateS)
                        tip=tr("Passer au moins une fois au Nord de cette ligne<br>")+
                            tr("Vous n'etes pas oblige de couper la ligne");
                    else
                    {
                        if(((QVariantMap)wps[str.setNum(i+1)].toMap()).isEmpty())
                            tip=tr("Arrivee<br>");
                        if(oneBuoy)
                        {
                            QString a;
                            if(qRound(wp["laisser_au"].toDouble()==wp["laisser_au"].toDouble()))
                                tip=tip+tr("Une seule bouee a laisser au ")+a.sprintf("%.0f",wp["laisser_au"].toDouble());
                            else
                                tip=tip+tr("Une seule bouee a laisser au ")+a.sprintf("%.2f",wp["laisser_au"].toDouble());
                        }
                        else
                            tip=tip+tr("Passage a deux bouees");
                        if(clockWise)
                            tip=tip+tr("<br>A passer dans le sens des aiguilles d'une montre");
                        else if (antiClockWise)
                            tip=tip+tr("<br>A passer dans le sens contraire des aiguilles d'une montre");
                        else
                            tip=tip+tr("<br>Sens du passage libre");
                        if(crossOnce)
                            tip=tip+tr("<br>Il est interdit de couper cette ligne plusieurs fois");
                        else
                            tip=tip+tr("<br>Il est autorise de couper cette ligne plusieurs fois");
                    }
                    porte->setTip(tip);
                    gates.append(porte);

                }
                gatesLoaded=true;
                updating=false;
                emit hasFinishedUpdating();
                emit boatUpdated(this,newRace,doingSync);
                showNextGates();
            }
            break;
        default:
            qWarning() << "[boatVLM-requestFinished] error: unknown request: " << getCurrentRequest();
            break;
    }
#if 0 //unflag to check b-vmg
    double res_h,res_wangle;
    if(this->polarData && lon != 0 && lat != 0 && WPLon != 0 && WPLat != 0)
    {
        this->polarData->bvmg(this->lon,this->lat,this->WPLon,this->WPLat,this->windDir,this->windSpeed,0,&res_h,&res_wangle);
        qWarning()<<"Cap BVMG: "<<res_h<<" TWA BVMG: "<<res_wangle;
    }
#endif
}

void boatVLM::authFailed(void)
{
    QMessageBox::warning(0,QObject::tr("Parametre bateau"),
                  QString("Erreur de parametrage du bateau '")+
                  pseudo+"'.\n Verifier le login et mot de passe puis reactivez le bateau");
    setStatus(false);
    emit boatUpdated(this,false,doingSync);
    inetClient::authFailed();
}

void boatVLM::inetError()
{
    updating=false;
    //emit hasFinishedUpdating();
    //emit boatUpdated(this,newRace,doingSync);
}

/**************************/
/* Select / unselect      */
/**************************/

void boatVLM::slot_selectBoat(void)
{
    boat::slot_selectBoat();
    showNextGates();
}


void boatVLM::unSelectBoat(bool needUpdate)
{
    boat::unSelectBoat(needUpdate);
    showNextGates();
}

/*************************************/
/* Drawing                           */

void boatVLM::showNextGates()
{
    if(!gatesLoaded) return;
    vlmLine * porte;
    QListIterator<vlmLine*> i (gates);
    int j=0;
    while(i.hasNext())
    {
        porte=i.next();
        if(!selected || porteHidden)
        {
            porte->setHidden(true);
            porte->hide();
            continue;
        }
        porte->show();
        j++;
        if(j<nWP)
        {
            porte->setHidden(true);
            porte->slot_showMe();
        }
        else if (j==nWP)
        {
            porte->setZValue(Z_VALUE_NEXT_GATE);
            QPen penLine(Settings::getSetting("nextGateLineColor", Qt::blue).value<QColor>(),1);
            penLine.setWidthF(Settings::getSetting("nextGateLineWidth", 3.0).toDouble());
            porte->setLinePen(penLine);
            porte->setHidden(false);
            porte->slot_showMe();
        }
        else
        {
            porte->setZValue(Z_VALUE_GATE);
            QPen penLine(Settings::getSetting("gateLineColor", Qt::magenta).value<QColor>(),1);
            penLine.setWidthF(Settings::getSetting("gateLineWidth", 3.0).toDouble());
            porte->setLinePen(penLine);
            porte->setHidden(false);
            porte->slot_showMe();
        }
    }

}

/**************************/
/* Data update            */
/**************************/

void boatVLM::updateBoatString()
{
    if(getAliasState() && !alias.isEmpty())
        my_str=alias;
    else
    {
        switch(Settings::getSetting("opp_labelType",0).toInt())
        {
            case SHOW_PSEUDO:
                my_str=pseudo;
                break;
            case SHOW_NAME:
                my_str=name;
                break;
            case SHOW_IDU:
                my_str=this->getBoatId();
                break;
        }
    }

     /* computing widget size */
    QFont myFont=QFont("Helvetica",9);
    QFontMetrics fm(myFont);
    prepareGeometryChange();
    width = fm.width("_"+my_str+"_")+2;
    height = qMax(fm.height()+2,12);
}

void boatVLM::updateHint(void)
{
    QString str;
    /* adding score */
    str = getScore() + " - Spd: " + QString().setNum(getSpeed()) + " - ";
    switch(getPilotType())
    {
        case 1: /*heading*/
            str += tr("Hdg") + ": " + getPilotString() + tr("deg");
            break;
        case 2: /*constant angle*/
            str += tr("Angle") + ": " + getPilotString()+ tr("deg");
            break;
        case 3: /*pilotortho*/
            str += tr("Ortho" ) + "-> " + getPilotString();
            break;
        case 4: /*VMG*/
            str += tr("BVMG") + "-> " + getPilotString();
            break;
       case 5: /*VBVMG*/
            str += tr("VBVMG") /*+ "-> " + getPilotString()*/;
            break;
    }
    QString desc;
    if(!polarData) desc=tr(" (pas de polaire chargee)");
    else if (polarData->getIsCsv()) desc=polarData->getName() + tr(" (format CSV)");
    else desc=polarData->getName() + tr(" (format POL)");
    str=str.replace(" ","&nbsp;");
    desc=desc.replace(" ","&nbsp;");
    setToolTip(desc+"<br>"+str);
}

void boatVLM::setStatus(bool activated)
{
    boat::setStatus(activated);
    if(!activated)
    {
        if(gatesLoaded)
        {
            while(!gates.isEmpty())
                delete gates.takeFirst();
        }
        gatesLoaded=false;
    }
}

void boatVLM::reloadPolar(void)
{
    if(forcePolar)
        polarName=polarForcedName;
    else
        polarName=polarVlm;
    boat::reloadPolar();
}
void boatVLM::setPolar(bool state,QString polar)
{
    this->polarForcedName=polar;
    forcePolar=state;
    if(activated)
        reloadPolar();
    else if(polarData)
    {
        emit releasePolar(polarData->getName());
        polarData=NULL;
    }

}

void boatVLM::setAlias(bool state,QString alias)
{
    useAlias=state;
    this->alias=alias;
}

QString boatVLM::getDispName(void)
{
    if(getAliasState() && !alias.isEmpty())
        return alias + " (" + pseudo + " - " + getBoatId() + ")";
    else
    {
        switch(Settings::getSetting("opp_labelType",0).toInt())
        {
            case SHOW_PSEUDO:
                return pseudo + " (" + name + " - " + getBoatId() +")";
                break;
            case SHOW_NAME:
                return name + " (" + pseudo + " - " + getBoatId() +")";
                break;
            case SHOW_IDU:
                return getBoatId() + " (" + pseudo + " - " + name +")";
                break;
        }
    }
    return "";
}