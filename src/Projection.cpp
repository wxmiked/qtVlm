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

#include <iostream>
#include <cmath>
#include <QDebug>
#include <QLine>
#include "Projection.h"

//-----------------------------
// Constructeur
//-----------------------------
Projection::Projection(int w, int h, double cx, double cy):
    W (0), H (0),
    CX (0), CY (0),
    xW (-90), xE (90), yN (90), yS (-90),
    PX (0), PY (0)
{
    frozen=false;
    scale = -1;
    my_setScreenSize(w, h);
    my_setCenterInMap(cx,cy);
    this->timer = new QTimer(this);
    assert(timer);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), this, SIGNAL(projectionUpdated()));
    useTempo=true;
}

void Projection::setScreenSize(int w, int h)
{
    my_setScreenSize(w,h);
    timer->start(5);
}

/* zoom and scale */
void Projection::zoomOnZone(const double &x0, const double &y0, const double &x1, const double &y1)
{
    double coef;

    if(frozen) return;

    xW=x0;
    xE=x1;
    yN=y0;
    yS=y1;
    // security
    if (xE == xW)
        xE = xW+0.1;
    if (yS == yN)
        yS = yN+0.1;

    //qWarning() << "init border : (" << xW << " " << xE << ") (" << yN << " " << yS << ")";

    // compute scale;
    double sX,sY,sYN,sYS;
    sX=W/fabs(xE-xW);
    sYN=log(tan(degToRad(yN)/2 + M_PI_4));
    sYS=log(tan(degToRad(yS)/2 + M_PI_4));
    sY=H/fabs(radToDeg(sYN-sYS));

    //qWarning() << "scale: X=" << sX << " Y=" << sY;
#if 0
    scale=sX<sY?sY:sX;
#else
    scale=sX>sY?sY:sX;
#endif
    //qWarning() << "Choosing: " << scale;

    if (scale > scalemax)
        scale = scalemax;
    if (scale<scaleall)
        scale = scaleall;
    //qWarning() << "final scale: " << scale;

    // Nouvelle position du centre
    PX = (xE+xW)/2;
    PY = ((radToDeg (log (tan (degToRad (yN)/2 + M_PI_4)))
           + radToDeg (log (tan (degToRad (yS)/2 + M_PI_4))))
          / 2);
    CX = PX;
    CY = radToDeg (2 * (atan (exp (degToRad (PY))) - M_PI_4));

    //qWarning() << "New center (" << CX << "," << CY << ") proj: (" << PX << "," << PY << ")";

    // compute new bounderies
#if 1
    coef=(W/2)/scale;
    xW=CX-coef;
    xE=CX+coef;

    //qWarning() << "New border x: " << xW << " " << xE;

    double xTmpW,xTmpE;
    screen2map(0,0,&xTmpW,&yN);
    screen2map(W,H,&xTmpE,&yS);

    //qWarning() << "New border y: " << yN << " " << yS;

    //qWarning() << "Double check x border: " << xTmpW << " " << xTmpE;
#endif

    if((getW()*getH())!=0)
        coefremp = 10000.0*fabs( ((xE-xW)*(yN-yS)) / (getW()*getH()) );
    else
        coefremp = 10000.0;

    emit newZoom(scale);
    emit_projectionUpdated();
}

void Projection::zoom(const double &k)
{
    if(frozen) return;
    my_setScale(scale*k);
    emit_projectionUpdated();
}
void Projection::zoomKeep(const double &lon, const double &lat, const double &k)
{    
    if(frozen) return;
    int xx,yy;
    map2screen(lon,lat,&xx,&yy);
    int centerX,centerY;
    map2screen(CX,CY,&centerX,&centerY);
    QLineF repere1(xx,yy,centerX,centerY);
    my_setScale(scale*k);
    map2screen(lon,lat,&xx,&yy);
    QLineF repere2(xx,yy,xx+1,yy+1);
    repere2.setAngle(repere1.angle());
    repere2.setLength(repere1.length());
    double ww,zz;
    screen2map(repere2.p2().x(),repere2.p2().y(),&ww,&zz);
    my_setCenterInMap(ww,zz);
    emit_projectionUpdated();
}

void Projection::zoomAll(void)
{
    if(frozen) return;

    my_setScale(scaleall);

    my_setCenterInMap(0,0);

    emit_projectionUpdated();
}

void Projection::setScale(const double &sc)
{
    if(frozen) return;
    my_setScale(sc);
    emit_projectionUpdated();
}

/**************************/
/* Move & center position */
/**************************/

void Projection::move(const double &dx, const double &dy)
{
    if(frozen) return;
    my_setCenterInMap(CX - dx*(xE-xW),CY - dy*(yN-yS));
    emit_projectionUpdated();
}

void Projection::setCentralPixel(const int &i, const int &j)
{
    if(frozen) return;
    double x, y;
    screen2map(i, j, &x, &y);
    my_setCenterInMap(x,y);
    emit_projectionUpdated();
}
void Projection::setCentralPixel(const QPointF &c)
{
    setCentralPixel(c.x(),c.y());
}

void Projection::setCenterInMap(const double &x, const double &y)
{
    if(frozen) return;
    my_setCenterInMap(x,y);
    emit_projectionUpdated();
}

void Projection::setScaleAndCenterInMap(const double &sc, const double &x, const double &y)
{
    if(frozen) return;
    bool mod=false;
    if(sc!=-1)
    {
        my_setScale(sc);
        mod=true;
    }

    if(!(x==0 && y==0))
    {
        my_setCenterInMap(x,y);
        mod=true;
    }

    if(mod)
        emit_projectionUpdated();
}

/**********************/
/* internal functions */
/**********************/

void Projection::my_setScale(double sc)
{
    scale = sc;
    if (scale < scaleall)
        scale = scaleall;
    if (scale > scalemax)
        scale = scalemax;
    updateBoundaries();
    emit newZoom(scale);
}

void Projection::my_setCenterInMap(double x, double y)
{
    while (x > 180.0) {
        x -= 360.0;
    }
    while (x < -180.0) {
        x += 360.0;
    }
    CX = x;
    CY = y;

    /* compute projection */
    PX=CX;
    PY=radToDeg(log(tan(degToRad(CY)/2 + M_PI_4)));

    updateBoundaries();
}

void Projection::my_setScreenSize(int w, int h)
{
    W = w;
    H = h;

    /* conpute scaleall */
    //qWarning() << "new MAP size: " << w << "," << h;
    double sx, sy,sy1,sy2;
    sx = W/359.9999;
    sy1=log(tan(degToRad(84)/2 + M_PI_4));
    sy2=log(tan(degToRad(-80)/2 + M_PI_4));
    sy = H/fabs(radToDeg(sy1-sy2));
    scaleall=qMax(sx,sy);
    if (scale < scaleall)
    {
        scale = scaleall;
        emit newZoom(scale);
    }

    updateBoundaries();
}

void Projection::updateBoundaries() {
    /* lat */
    yS=radToDeg(2*atan(exp((double)(degToRad(PY-H/(2*scale)))))-M_PI_2);
    yN=radToDeg(2*atan(exp((double)(degToRad(PY+H/(2*scale)))))-M_PI_2);

    /* lon */
    xW=PX-W/(2*scale);
    xE=PX+W/(2*scale);

    //qWarning() << "Updated longitude range:" << xW << "-" << xE;
    //qWarning() << "Updated latitude range :" << yN << "-" << yS;

    /* xW and yN => upper corner */

    if((getW()*getH())!=0)
        coefremp = 10000.0*fabs( ((xE-xW)*(yN-yS)) / (getW()*getH()) );
    else
        coefremp = 10000.0;
}
void Projection::emit_projectionUpdated()
{
    if(useTempo)
        timer->start(200);
    else
        emit projectionUpdated();
}

