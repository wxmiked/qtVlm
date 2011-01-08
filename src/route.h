/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2008 - Christophe Thomas aka Oxygen77

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

Original code: zyGrib: meteorological GRIB file viewer
Copyright (C) 2008 - Jacques Zaninetti - http://zygrib.free.fr

***********************************************************************/
#ifndef ROUTE_H
#define ROUTE_H

#include <QPainter>
#include <QGraphicsWidget>
#include <QGraphicsScene>
#include <QMenu>
#include <QDateTime>
#include "mycentralwidget.h"

#include "class_list.h"


//===================================================================
class ROUTE : public QObject
{ Q_OBJECT
    public:
        /* constructeurs, destructeurs */
        ROUTE(QString name, Projection *proj, Grib *grib, QGraphicsScene * myScene, myCentralWidget *parentWindow);

        ~ROUTE();

        void setName(QString name){this->name=name;}
        QString getName() {return(this->name);}

        void setBoat(boat *boat);
        boat * getBoat(){return this->myBoat;}

        void setColor(QColor color);
        QColor getColor(){return this->color;}

        void setWidth(float width);
        float getWidth() {return this->width;}

        void setStartTime(QDateTime date){this->startTime=date;}
        QDateTime getStartTime() {return this->startTime.toUTC();}

        void setFrozen(bool frozen) {if(frozen==this->frozen) return;this->frozen=frozen;if(frozen==false) {imported=false;slot_recalculate();}}
        void setFrozen2(bool frozen) {if(frozen==this->frozen) return;this->frozen=frozen;if(!frozen)slot_recalculate();}
        void setFrozen3(bool frozen){this->frozen=frozen;}
        bool getFrozen(){return this->frozen;}
        void setLive(bool live) {this->live=live;}
        bool isLive(){return this->live;}
        bool isBusy(){return this->busy;}
        void setBusy(bool b){this->busy=b;}


        void setStartFromBoat(bool startFromBoat){this->startFromBoat=startFromBoat;}
        bool getStartFromBoat() {return this->startFromBoat;}

        void setStartTimeOption(int startTimeOption){this->startTimeOption=startTimeOption;}
        int getStartTimeOption() {return this->startTimeOption;}

        static bool myLessThan(ROUTE * ROUTE_1,ROUTE* ROUTE_2) {return ROUTE_1->name < ROUTE_2->name;}
        static bool myEqual(ROUTE * ROUTE_1,ROUTE* ROUTE_2) {return ROUTE_1->name == ROUTE_2->name;}

        void insertPoi(POI *poi);
        void removePoi(POI *poi);
        POI * getFirstPoi(){return this->my_poiList.first();}
        POI * getLastPoi(){return this->my_poiList.last();}
        time_t getEta(){return this->eta;}
        time_t getStartDate(){return this->start;}
        bool getHas_eta(){return this->has_eta;}
        double getRemain(){return this->remain;}
        bool isPartOfBvmg(POI * poi);
        void setOptimizing(bool b){this->optimizing=b;}
        bool getOptimizing(){return optimizing;}
        void setFastVmgCalc(bool b){this->fastVmgCalc=b;}
        void setOptimizingPOI(bool b){this->optimizingPOI=b;hasStartEta=false;startPoiName="";}
        void setPoiName(QString name){this->poiName=name;}
        float getStartLat(){return this->startLat;}
        float getStartLon(){return this->startLon;}
        bool getHidePois(){return this->hidePois;}
        void setHidePois(bool b);
        bool isImported(){return imported;}
        void setImported(){this->imported=true;}
        void setMultVac(int i){this->multVac=i;}
        int getMultVac(){return this->multVac;}
        QList<POI*> & getPoiList() { return this->my_poiList; }
        vlmLine * getLine(){return this->line;}
        bool getSimplify(){return this->simplify;}
        void setSimplify(bool b){this->simplify=b;}
        void setHidden(bool b){if(b==hidden) return;this->hidden=b;this->slot_shShow();}
        bool getHidden(void){return this->hidden;}
        bool getUseVbvmgVlm(void){return this->useVbvmgVlm;}
        void setUseVbVmgVlm(bool b){this->useVbvmgVlm=b;}
        void setTemp(bool b){this->temp=b;}
        bool getTemp(){return this->temp;}

    public slots:
        void slot_recalculate(boat * boat=NULL);
        void slot_edit();
        void slot_shShow();
        void slot_shHidden();
        void slot_shRou(){if(!this->hidden) slot_shHidden();else slot_shShow();}
        void slot_export(){parent->exportRouteFromMenu(this);}
        void slot_boatPointerHasChanged(boat * acc);
        void slot_compassFollow(){parent->setCompassFollow(this);}
    signals:
        void editMe(ROUTE *);
    private:
        /* parent, main */
        myCentralWidget *parent;
        QGraphicsScene *myscene;
        Projection *proj;

        /* widget component */
        QColor color;
        float width;
        vlmLine *line;
        QPen pen;

        /* data */
        QString name;
        QList<POI*> my_poiList;
        boat *myBoat;
        QString boatLogin;
        Grib *grib;
        bool startFromBoat;
        QDateTime startTime;
        int startTimeOption;
        bool frozen;
        bool superFrozen;
        bool live;
        bool busy;

        /*popup menu*/
        QMenu *popup;
        QAction *r_edit;
        QAction *r_hide;
        QAction *r_delete;
        void createPopUpMenu(void);

        /*various*/
        double A360(double hdg);
        double A180(double hdg);
        double myDiffAngle(double a1,double a2);
        time_t eta;
        time_t start;
        bool has_eta;
        double remain;
        bool optimizing;
        bool fastVmgCalc;
        bool optimizingPOI;
        QString poiName;
        QString startPoiName;
        bool hasStartEta;
        time_t startEta;
        float startLon;
        float startLat;
        bool hidePois;
        void interpolatePos();
        bool imported;
        int multVac;
        bool simplify;
        bool hidden;
        void do_vbvmg_context(double dist,double wanted_heading,
                              double w_speed,double w_angle,
                              double *heading1, double *heading2,
                              double *wangle1, double *wangle2,
                              double *time1, double *time2,
                              double *dist1, double *dist2);
        bool useVbvmgVlm;
        bool initialized;
        bool temp;
};
#endif // ROUTE_H