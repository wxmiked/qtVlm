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
***********************************************************************/

#include <QDebug>

#include "race_dialog.h"
#include "Util.h"

#define RACE_NO_REQUEST 0
#define RACE_LIST_BOAT  1

race_dialog::race_dialog(QWidget * main,QWidget * parent) : QDialog(parent)
{
    setupUi(this);
    chooser_raceList->setInsertPolicy(QComboBox::InsertAlphabetically);
    availableBoat->setSortingEnabled(true);
    availableBoat->setSelectionMode(QAbstractItemView::ExtendedSelection);
    selectedBoat->setSortingEnabled(true);
    selectedBoat->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(this,SIGNAL(readBoat()),main,SLOT(slotReadBoat()));
    connect(this,SIGNAL(writeBoat()),main,SLOT(slotWriteBoat()));

    /* init http inetManager */
    conn=new inetConnexion(main,this);

    waitBox = new QMessageBox(QMessageBox::Information,
                             tr("Paramétrage des courses"),
                             tr("Chargement des courses et des bateaux"),
                             QMessageBox::Cancel,this
                             );
}

race_dialog::~race_dialog()
{

}

void race_dialog::initList(QList<boatAccount*> & acc_list_ptr,QList<raceData*> & race_list_ptr)
{
    this->acc_list = & acc_list_ptr;
    this->race_list = & race_list_ptr;

    qWarning() << "init list";

    numRace = -1;
    initDone = false;

    clear();

    /* determines the list of race */
    for(int i=0;i<acc_list->size();i++)
    {
        bool found = false;

        //qWarning() << acc_list->at(i)->getLogin() << " " << acc_list->at(i)->getStatus() << " " << acc_list->at(i)->getRaceId();

        if(!acc_list->at(i)->getStatus() || acc_list->at(i)->getRaceId() == "0")
            continue;

        for(int j=0;j<param_list.size();j++)
            if(param_list[j]->id == acc_list->at(i)->getRaceId())
            {
                found=true;
                break;
            }
        if(!found) /* not adding a race twice*/
        {
            raceParam * ptr = new raceParam();
            ptr->id=acc_list->at(i)->getRaceId();
            ptr->name=acc_list->at(i)->getRaceName();
            ptr->boats.clear();
            param_list.append(ptr);
        }
    }
    /* init combo box with list of races */
    chooser_raceList->clear();

    for(int j=0;j<param_list.size();j++)
        chooser_raceList->addItem(param_list[j]->name,param_list[j]->id);
    nbRace->setText(QString().setNum(param_list.size()));
    /* get boat list */
    if(!conn)
    {
        qWarning() << "No connection structure available";
        return ;
    }

    if(!conn->isAvailable() )
    {
        qWarning() << "request already running" ;
        return ;
    }

    /* init index of search : currentRace*/
    currentRace=-1;
    getNextRace();

    initDone = true;

    waitBox->exec();
    #warning should cancel current request if cancel is pressed
}

void race_dialog::getNextRace()
{
    currentRace++;
    if(currentRace>=param_list.size())
    {
        /* finished */
        waitBox->hide();
        numRace=-1;
        chgRace(0);
        return;
    }

    /* let's find this race in the param from boatAcc.dat*/
    currentParam.clear();
    for(int i=0;i<race_list->size();i++)
        if(race_list->at(i)->idrace==param_list[currentRace]->id)
        {
            currentParam=race_list->at(i)->oppList.split(";");
            break;
        }

    QString page;
    QTextStream(&page) << "/getuserlist.php?"
                        << "idr="
                        << param_list[currentRace]->id;

    conn->doRequestGet(RACE_LIST_BOAT,page);
}

void race_dialog::requestFinished (int ,QByteArray data)
{
    QString strbuf(data);
    QStringList list_res;
    QStringList boat;
    int i;
    struct boatParam * ptr;

    list_res=strbuf.split("\n");
    for(i=5;i<list_res.size();i++)
    {
        boat=list_res[i].split(";");
        if(boat.size() < 5) /* not enough data => next line */
        {
            continue;
        }
        ptr = new boatParam();
        ptr->login=boat[1];
        /* ex: 6936;Aarizia 4;Petit Navire;Nova_Scotia;79.94.105.194 */
        ptr->name=boat[2];
        ptr->user_id=boat[0];
        ptr->selected=currentParam.contains(boat[0]);
        param_list[currentRace]->boats.append(ptr);
    }
    /* next race */
    getNextRace();
}

void race_dialog::clear(void)
{
    for(int i=0;i<param_list.size();i++)
    {
        for(int j=0;j<param_list[i]->boats.size();j++)
            delete param_list[i]->boats[j];
        param_list[i]->boats.clear();
        delete param_list[i];
    }
    param_list.clear();
}

void race_dialog::done(int result)
{
    if(result == QDialog::Accepted)
        saveData(true); /* really saving, not only applying*/
    else
    {
        qWarning() << "Cancel modification: read boat param from disk";
        emit readBoat();
    }
    QDialog::done(result);
}

void race_dialog::doSynch(void) /* apply only */
{
    int savNum=numRace;
    saveData(false);
    chgRace(savNum);
}

void race_dialog::saveData(bool save)
{
    QStringList boats;
    struct raceData * ptr;
    /* removing all races */
    for(int i=0;i<race_list->size();i++)
        delete race_list->at(i);
    race_list->clear();

    /* forcing sync of currently displayed race*/
    chgRace(-1);

    /* saving races */
    for(int i=0;i<param_list.size();i++)
    {
        boats.clear();
        for(int j=0;j<param_list[i]->boats.size();j++)
            if(param_list[i]->boats[j]->selected)
                boats.append(param_list[i]->boats[j]->user_id);

        ptr = new raceData();
        ptr->idrace=param_list[i]->id;
        if(!boats.isEmpty())
            ptr->oppList=boats.join(";");
        else
            ptr->oppList=boats.join(";");
        race_list->append(ptr);
    }

    if(save)
        emit writeBoat();
}

void race_dialog::chgRace(int id)
{
    if(!initDone)
        return;

    if(numRace!=-1)
    {
        /* changement du select des boat */
        struct boatParam * ptr;
        for(int i=0;i<selectedBoat->count();i++)
        {
            ptr=reinterpret_cast<struct boatParam *>(qvariant_cast<void*>(selectedBoat->item(i)->data(Qt::UserRole)));
            ptr->selected=true;
        }
    }

    /* find race data */
    if(id < 0 || id >= chooser_raceList->count())
    {
        qWarning() << "chgRace: Bad id :" << id;
        return;
    }

    QString idRace = chooser_raceList->itemData(id).toString();

    for(numRace=0;numRace<param_list.size();numRace++)
        if(param_list[numRace]->id == idRace)
            break;
    if(numRace==param_list.size())
    {
        qWarning() << "chgRace: id not found";
        return;
    }

    availableBoat->clear();
    selectedBoat->clear();

    QListWidgetItem * ptr;
    for(int i=0;i<param_list[numRace]->boats.size();i++)
    {
        ptr=new QListWidgetItem(param_list[numRace]->boats[i]->login + " - " + param_list[numRace]->boats[i]->user_id);
        ptr->setData(Qt::UserRole,QVariant(QMetaType::VoidStar, &param_list[numRace]->boats[i]));
        if(param_list[numRace]->boats[i]->selected)
            selectedBoat->addItem(ptr);
        else
            availableBoat->addItem(ptr);
        param_list[numRace]->boats[i]->selected=false;
    }

    nbAvailable->setText(QString().setNum(availableBoat->count()));
    nbSelect->setText(QString().setNum(selectedBoat->count())+"/"+QString().setNum(RACE_MAX_BOAT));

    availableBoat->clearSelection();
    selectedBoat->clearSelection();
}

void race_dialog::addBoat(void)
{
    mvBoat(availableBoat,selectedBoat,true);
}

void race_dialog::delBoat(void)
{
    mvBoat(selectedBoat,availableBoat,false);
}

void race_dialog::mvBoat(QListWidget * from,QListWidget * to,bool withLimit)
{
    QList<QListWidgetItem *> selItem = from->selectedItems();
    QListWidgetItem * item;
    QListIterator<QListWidgetItem*> i (selItem);
    while(i.hasNext())
    {
        if(withLimit && selectedBoat->count()>=RACE_MAX_BOAT)
        {
               QMessageBox::warning(this,tr("Paramétrage des courses"),
                                     tr("Nombre maximum de concurrent dépassé")+" ("
                                     +QString().setNum(RACE_MAX_BOAT)+")");
               break;
        }
        item = i.next();
        from->takeItem(from->row(item));
        to->addItem(item);
    }
    nbAvailable->setText(QString().setNum(availableBoat->count()));
    nbSelect->setText(QString().setNum(selectedBoat->count())+"/"+QString().setNum(RACE_MAX_BOAT));
    availableBoat->clearSelection();
    selectedBoat->clearSelection();
}

/*void race_dialog::addAllBoat(void)
{
    mvAllBoat(availableBoat,selectedBoat);
}*/

void race_dialog::delAllBoat(void)
{
    mvAllBoat(selectedBoat,availableBoat);
}

void race_dialog::mvAllBoat(QListWidget * from,QListWidget * to)
{
    while(from->count())
        to->addItem(from->takeItem(0));
    nbAvailable->setText(QString().setNum(availableBoat->count()));
    nbSelect->setText(QString().setNum(selectedBoat->count())+"/"+QString().setNum(RACE_MAX_BOAT));
    availableBoat->clearSelection();
    selectedBoat->clearSelection();
}

