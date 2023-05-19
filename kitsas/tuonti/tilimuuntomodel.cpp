/*
   Copyright (C) 2018 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "tilimuuntomodel.h"
#include "db/tilinvalintadialogi.h"
#include "db/kirjanpito.h"

#include "ui_tilimuuntodlg.h"
#include "kirjaus/tilidelegaatti.h"
#include "kirjaus/eurodelegaatti.h"

#include <QPalette>

TilinMuunnos::TilinMuunnos(int numero, QString nimi, int muunnettu, Euro saldo)
    : alkuperainenTilinumero_(numero), tilinNimi_(nimi), muunnettuTilinumero_{muunnettu}, saldo_{saldo}
{

}

QString TilinMuunnos::tiliStr() const
{
    if( alkuperainen() )
        return QString::number(alkuperainen());
    else
        return tiliNimi();
}


void TilinMuunnos::setMuunnettu(int tilinumero)
{
    muunnettuTilinumero_ = tilinumero;
}

void TilinMuunnos::setSaldo(const Euro &saldo)
{
    saldo_ = saldo;
}



TiliMuuntoModel::TiliMuuntoModel(QObject *parent) :
    QAbstractTableModel{parent}
{

}

TiliMuuntoModel::TiliMuuntoModel(const QList<QPair<int, QString>> &tilit)
{
    // Luetaan asetuksista aiemmat muunnot
    const QStringList& aiempiMuuntoLista = kp()->asetukset()->lista("TiliMuunto");

    for(const auto& rivi : qAsConst(aiempiMuuntoLista))
    {
        int vali = rivi.indexOf(' ');
        if( vali < 1)
            continue;
        int tili = rivi.left(vali).toInt();
        if( tili )
        {
            muunteluLista_.insert(rivi.mid(vali+1), tili);
        }
    }

    for( auto pari : tilit)
    {
       int muunnettu = muunteluLista_.value(pari.second, pari.first);
       if( !kp()->tilit()->tiliNumerolla(muunnettu).onkoValidi())
           muunnettu = 0;

       data_.append(TilinMuunnos(pari.first, pari.second, muunnettu));
    }
}

int TiliMuuntoModel::rowCount(const QModelIndex & /* parent */) const
{
    return data_.count();
}

int TiliMuuntoModel::columnCount(const QModelIndex& /* parent */) const
{
    return moodi_ == TUONTI_VAIN_TILIT ? 3 : 4;
}

QVariant TiliMuuntoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section) {
        case ALKUPERAINEN:
            return tr("Numero");
        case NIMI:
            return tr("Nimi");
        case UUSI:
            return tr("Kirjataan tilille");
        case SALDO:
            return tr("Saldo");
        }
    }

    return QVariant();

}

QVariant TiliMuuntoModel::data(const QModelIndex &index, int role) const
{
    if( !index.isValid())
        return QVariant();

    TilinMuunnos rivi = data_.value(index.row());

    if( role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column()) {
        case ALKUPERAINEN:
           return rivi.alkuperainen();
        case NIMI:
            return rivi.tiliNimi();
        case UUSI: {
            if( role == Qt::EditRole)
                return rivi.muunnettu();

            Tili tili= kp()->tilit()->tiliNumerolla( rivi.muunnettu() );
            if( tili.onkoValidi())
                return tili.nimiNumero();
            return QString();
        }
        case SALDO:
            if( role == Qt::DisplayRole)
                return rivi.saldo().display();
            else
                return rivi.saldo().toString();
        }

    }
    if( role == Qt::ForegroundRole && index.column() == UUSI) {
        Tili tili= kp()->tilit()->tiliNumerolla( rivi.muunnettu() );
        if( tili.onkoValidi()) {
            if(tili.nimi() == rivi.tiliNimi())
                return QPalette().base().color().lightness() > 128 ? QColor(Qt::darkGreen) : QColor(Qt::green);
            else if(rivi.alkuperainen() != rivi.muunnettu())
                return QPalette().base().color().lightness() > 128 ? QColor(Qt::darkMagenta) : QColor(Qt::magenta);
        }
    }

    return QVariant();
}

bool TiliMuuntoModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if( index.column() == UUSI && role == Qt::EditRole)
    {
        if( value.toInt()) {
            data_[index.row()].setMuunnettu(value.toInt());
            emit dataChanged(index, index, QVector<int>() << role);
            return true;
        }
    } else if( index.column() == SALDO && role == Qt::EditRole) {
        data_[index.row()].setSaldo(value.toString());
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    return false;
}

Qt::ItemFlags TiliMuuntoModel::flags(const QModelIndex &index) const
{
    if( !index.isValid() || index.column() == ALKUPERAINEN || index.column() == NIMI)
        return QAbstractTableModel::flags(index);

    TilinMuunnos rivi = data_.value(index.row());
    if(index.column() == UUSI || ( index.column() == SALDO && moodi_ == TILINAVAUS_MUOKKAA_SALDO ))
        return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
    else
        return QAbstractTableModel::flags(index);
}

int TiliMuuntoModel::tilinumeroIndeksilla(int indeksi) const
{
    return data_.at(indeksi).muunnettu();
}


QMap<QString, int> TiliMuuntoModel::muunnettu()
{
    QMap<QString,int> tulos;
    for( const auto& rivi :  qAsConst( data_ ))
    {        
        tulos.insert( rivi.tiliStr(), rivi.muunnettu());
        muunteluLista_.insert( rivi.tiliStr(), rivi.muunnettu() );
    }

    // Lopuksi päivitetään muuntotaulukko
    QStringList listaan;
    auto iter = QMapIterator<QString,int>(muunteluLista_);
    while(iter.hasNext())
    {
        iter.next();
        listaan.append( QString("%1 %2").arg(iter.value()).arg(iter.key()) );
    }

    kp()->asetukset()->aseta("TiliMuunto", listaan);


    return tulos;
}

int TiliMuuntoModel::muunnettu(int alkuperainen) const
{
    for(const auto& tili : data_) {
        if( tili.alkuperainen() == alkuperainen)
            return tili.muunnettu();
    }
    return 0;
}


void TiliMuuntoModel::lisaa(int numero, const QString &nimi, Euro saldo)
{
    // Etsitään ensin, onko tämä jo
    for(int i=0; i < rowCount(); i++) {
        if( data_.at(i).alkuperainen() == numero) {
            data_[i].setSaldo( data_.at(i).saldo() + saldo );
            return;
        }
    }

    Tili tiliNumerolla = kp()->tilit()->tiliNumerolla(numero);
    int tilinumero = (tiliNumerolla.onkoValidi() && tiliNumerolla.nimi() == nimi)
            ? numero : 0;
    QString vertailunimi = nimi.toLower();
    vertailunimi.remove(TyhjaPoisRE__);

    if( !tilinumero) {
        // Yritetään löytää sama tilinumero tilikartasta nimeä etsimällä
        for(int i=0; i < kp()->tilit()->rowCount(); i++) {
            Tili* ptili = kp()->tilit()->tiliPIndeksilla(i);
            QString tilinimi = ptili->nimi().toLower();
            tilinimi.remove(QRegularExpression(TyhjaPoisRE__));
            if( !ptili->otsikkotaso() && vertailunimi == tilinimi) {
                QString numerostr = QString::number(numero);
                if( numerostr > "3" || numerostr.at(0) == ptili->nimiNumero().at(0)  ) {
                    tilinumero = ptili->numero();
                    break;
                }
            }
        }
    }

    if(!tilinumero && numero && kp()->tilit()->tili(numero))
        tilinumero = numero;

    if(!tilinumero) {   // Haetaan vielä tilinimen osalla
        for(int i=0; i < kp()->tilit()->rowCount(); i++) {
            Tili* ptili = kp()->tilit()->tiliPIndeksilla(i);
            QString tilinimi = ptili->nimi().toLower();
            tilinimi.remove(QRegularExpression(TyhjaPoisRE__));
            if( !ptili->otsikkotaso() && ( vertailunimi.contains(tilinimi) || tilinimi.contains(vertailunimi) )) {
                QString numerostr = QString::number(numero);
                if( (numerostr > "3" && ptili->nimiNumero() > "3") || numerostr.at(0) == ptili->nimiNumero().at(0)  ) {
                    tilinumero = ptili->numero();
                    break;
                }
            }
        }
    }

    beginInsertRows(QModelIndex(), data_.count(), data_.count());
    data_.append( TilinMuunnos(numero, nimi, tilinumero, saldo) );
    endInsertRows();
}

bool TiliMuuntoModel::naytaMuuntoDialogi(QWidget *parent)
{    

    QDialog muuntoDlg(parent);
    Ui::TiliMuunto mui;
    mui.setupUi(&muuntoDlg);
    mui.muuntoView->setModel(this);

    TiliDelegaatti* delegaatti = new TiliDelegaatti(this);
    delegaatti->etsiKayttoon(false);
    delegaatti->naytaKaikki();
    mui.muuntoView->setItemDelegateForColumn( UUSI , delegaatti);
    mui.muuntoView->setItemDelegateForColumn( SALDO, new EuroDelegaatti(this));
    mui.muuntoView->horizontalHeader()->setSectionResizeMode(NIMI, QHeaderView::Stretch);
    mui.muuntoView->horizontalHeader()->setSectionResizeMode(UUSI, QHeaderView::Stretch);

    if( moodi_ == TUONTI_VAIN_TILIT) {
        connect( mui.ohjeNappi, &QPushButton::clicked, []{ kp()->ohje("kirjaus/tuonti/");});
    } else {
        mui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled( this->kaikkiMuunnettu() );
        connect( mui.ohjeNappi, &QPushButton::clicked, []{ kp()->ohje("asetukset/tilinavaus/");});
        connect( this, &TiliMuuntoModel::dataChanged, [this, mui] () {
            mui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled( this->kaikkiMuunnettu() );
        });
        mui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled( kaikkiMuunnettu() );
    }    

    return( muuntoDlg.exec() == QDialog::Accepted);

}


bool TiliMuuntoModel::kaikkiMuunnettu() const
{
    for(const auto& item: qAsConst(data_)) {
        if( !item.muunnettu())
            return false;
    }
    return true;
}

void TiliMuuntoModel::asetaMoodi(TiliMuuntoMoodi moodi)
{
    beginResetModel();
    moodi_ = moodi;
    endResetModel();
}

QRegularExpression TiliMuuntoModel::TyhjaPoisRE__ = QRegularExpression("\\W+");

