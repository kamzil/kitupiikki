/*
   Copyright (C) 2019 Arto Hyvättinen

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
#include "tositerivit.h"

#include "db/verotyyppimodel.h"
#include "db/kohdennus.h"
#include "db/kirjanpito.h"
#include "db/tilinvalintadialogi.h"

#include "model/tositevienti.h"
#include "lasku.h"

#include <QMessageBox>

TositeRivit::TositeRivit(QObject *parent, const QVariantList& data)
    : QAbstractTableModel(parent)
{
    for(const auto& variant : data) {
        rivit_.append( TositeRivi( variant.toMap()) );
    }
}

void TositeRivit::lataa(const QVariantList &data)
{
    beginResetModel();
    rivit_.clear();

    for(const auto& variant : data) {
        rivit_.append( TositeRivi( variant.toMap()) );
    }

    endResetModel();
}

QVariant TositeRivit::headerData(int section, Qt::Orientation orientation, int role) const
{

    if( role == Qt::DisplayRole)
    {
        if( orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case NIMIKE:
                    return tr("Nimike");
                case MAARA:
                    return tr("Määrä");
                case YKSIKKO:
                    return tr("Yksikkö");
                case AHINTA :
                    return tr("á netto");
                case ALE:
                    return  tr("Alennus");
                case ALV:
                    return tr("Alv");
                case TILI:
                    return tr("Tili");
                case KOHDENNUS:
                    return tr("Kohdennus");
                case BRUTTOSUMMA:
                    return tr("Yhteensä");
            }
        }
        else
            return QVariant( section + 1);
    }
    return QVariant();
}


int TositeRivit::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return rivit_.count();
}

int TositeRivit::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 9;
}

QVariant TositeRivit::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const TositeRivi& rivi = rivit_.at(index.row());


    if( role == Qt::TextAlignmentRole)
    {
        if( index.column()==BRUTTOSUMMA || index.column() == MAARA || index.column() == ALV || index.column() == AHINTA || index.column() == ALE)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        else
            return QVariant( Qt::AlignLeft | Qt::AlignVCenter);

    } else if( role == Qt::DisplayRole) {
        switch (index.column()) {
        case NIMIKE:
            return rivi.nimike();
        case MAARA:
            return rivi.laskutetaanKpl().replace(".",",");
        case YKSIKKO:
            if( rivi.unKoodi().isEmpty())
                return rivi.yksikko();
            else
                return yksikkoModel_.nimi(rivi.unKoodi());
        case AHINTA:
            return QString("%L1 €").arg( rivi.aNetto(),0,'f',2);
        case ALE:
            if(rivi.aleProsentti())
                return QString("%1 %").arg(rivi.aleProsentti() );
            else if( rivi.euroAlennus())
                return rivi.euroAlennus().display();
            else
                return QString();
        case ALV:
            switch ( rivi.alvkoodi() ) {
            case AlvKoodi::ALV0:
                return tr("0 %");
            case AlvKoodi::RAKENNUSPALVELU_MYYNTI:
                return tr("AVL 8c §");
            case AlvKoodi::YHTEISOMYYNTI_PALVELUT:
                return tr("AVL 65 §");
            case AlvKoodi::YHTEISOMYYNTI_TAVARAT:
                return tr("AVL 72a §");
            case Lasku::KAYTETYT:
                return tr("Margin. tavarat","Marginaaliverotus käytetyt tavarat/ Lyhyt teksti laskunäkymässä");
            case Lasku::TAIDE:
                return tr("Margin. taide","Marginaaliverotus taide-esineet/ Lyhyt teksti laskunäkymässä");
            case Lasku::ANTIIKKI:
                return tr("Margin. keräily","Marginaaliverotus keräily- ja antiikkiesineet/ Lyhyt teksti laskunäkymässä");
            default:
                return QString("%1 %").arg( rivi.alvProsentti() );
            }
        case KOHDENNUS:
            {
                Kohdennus kohdennus = kp()->kohdennukset()->kohdennus(  rivi.kohdennus() );
                if( kohdennus.tyyppi() != Kohdennus::EIKOHDENNETA)
                    return kohdennus.nimi();
                else
                    return QVariant();
        }
        case TILI:
            if( ennakkolasku_ )
                return tr("Saadut ennakot");
            return kp()->tilit()->tiliNumerolla( rivi.tili() ).nimiNumero();
        case BRUTTOSUMMA:
           return rivi.bruttoYhteensa().display();
        }
    } else if( role == Qt::EditRole) {
        switch (index.column()) {
        case NIMIKE:
            return rivi.nimike();
        case MAARA:
            return rivi.laskutetaanKpl();
        case YKSIKKO:
            return rivi.yksikko();
        case AHINTA:
            return rivi.aNetto();
        case ALE:
            if( rivi.aleProsentti()) {
                return rivi.aleProsentti();
            } else if( rivi.euroAlennus() + rivi.bruttoYhteensa()) {
                    return ( 100 *rivi.euroAlennus().cents() /
                             (rivi.euroAlennus().cents() + rivi.bruttoYhteensa().cents()) );
            } else {
                return QVariant();
            }
        case KOHDENNUS:
            return rivi.kohdennus();
        case TILI:
            return rivi.tili();
        case BRUTTOSUMMA:
            return rivi.bruttoYhteensa();
        }
    } else if( role == AlvProsenttiRooli)
        return rivi.alvProsentti();
    else if( role == AlvKoodiRooli)
        return rivi.alvkoodi();
    else if( role == TiliNumeroRooli )
        return rivi.tili();
    else if( role == UNkoodiRooli)
        return rivi.unKoodi();
    else if( role == Qt::DecorationRole && index.column() == ALV) {
        return kp()->alvTyypit()->kuvakeKoodilla(rivi.alvkoodi());
    }

    // FIXME: Implement me!
    return QVariant();
}

bool TositeRivit::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value) {        
        int r = index.row();

        if( role == Qt::EditRole) {
            switch (index.column()) {
            case NIMIKE:
                rivit_[r].setNimike( value.toString() );
                break;
            case MAARA:
                rivit_[r].setLaskutetaanKpl( value.toString());
                rivit_[r].setMyyntiKpl(value.toDouble());
                rivit_[r].laskeYhteensa();
                break;
            case YKSIKKO:
                rivit_[r].setYksikko( value.toString() );
                rivit_[r].setUNkoodi("ZZ");
                break;
            case AHINTA:
                rivit_[r].setANetto( value.toDouble());
                rivit_[r].laskeYhteensa();
                break;
            case ALE:
                rivit_[r].setEuroAlennus(Euro());
                rivit_[r].setAleProsentti(value.toDouble());
                rivit_[r].laskeYhteensa();
                break;
            case KOHDENNUS:
                rivit_[r].setKohdennus(value.toInt());
                break;
            case TILI:
            {
                // Tili asetetaan numerolla!
                Tili uusitili;
                if( value.toInt() )
                    uusitili = kp()->tilit()->tiliNumerolla( value.toInt());
                else if(!value.toString().isEmpty() && value.toString() != " " && value.toString() != "0")
                    uusitili = TilinValintaDialogi::valitseTili(value.toString());
                else if( value.toString()==" " || !rivit_.at(r).tili() )
                    uusitili = TilinValintaDialogi::valitseTili( QString());

                if( uusitili.onkoValidi() && (uusitili.alvlaji() == AlvKoodi::EIALV ||
                                              uusitili.alvlaji() == AlvKoodi::MYYNNIT_NETTO ) ) {
                    rivit_[r].setTili(uusitili.numero());
                    if( kp()->asetukset()->onko(AsetusModel::AlvVelvollinen)) {
                        rivit_[r].setAlvKoodi( uusitili.alvlaji() );
                        rivit_[r].setAlvProsentti( uusitili.alvprosentti() );
                        rivit_[r].laskeYhteensa();
                    }
                }
            }
                break;
            case BRUTTOSUMMA:
                rivit_[r].setBruttoYhteensa( value.toString() );
                rivit_[r].laskeYksikko();
            }
        } else if( role == AlvKoodiRooli) {
            rivit_[r].setAlvKoodi(value.toInt());
            rivit_[r].laskeYhteensa();            
        } else if( role == AlvProsenttiRooli) {
            rivit_[r].setAlvProsentti(value.toDouble());
            rivit_[r].laskeYhteensa();
        } else if( role == UNkoodiRooli) {
            rivit_[r].setUNkoodi(value.toString());
            rivit_[r].setYksikko(QString());    // TODO! Yksikön tulkattu teksti?
        }

        emit dataChanged(index.sibling(index.row(), MAARA),
                             index.sibling(index.row(), BRUTTOSUMMA),
                             QVector<int>() << Qt::DisplayRole << Qt::EditRole );

        if( index.row() == rowCount() - 1 && rivit_.at(r).bruttoYhteensa().cents())
            lisaaRivi();

        return true;
    }
    return false;
}

Qt::ItemFlags TositeRivit::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    if( ennakkolasku_ && index.row() == TILI)
        return QAbstractTableModel::flags(index) | Qt::ItemIsEnabled;

    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

QVariantList TositeRivit::rivit() const
{
    QVariantList ulos;
    for(const auto& rivi : rivit_)
    {
        if( rivi.bruttoYhteensa().cents() || !rivi.nimike().isEmpty())
            ulos.append( rivi.data() );
    }
    return ulos;
}

Euro TositeRivit::yhteensa() const
{
    Euro summa;
    for(auto rivi : rivit_)
        summa += rivi.bruttoYhteensa();
    return summa;
}

QVariantList TositeRivit::viennit(const Tosite &tosite) const
{
    return QVariantList();

    /*
    QVariantList lista;
    const Lasku& lasku = tosite.constLasku();
    const QDate pvm = lasku.maksutapa() == Lasku::SUORITEPERUSTE
            ? lasku.toimituspvm()
            : tosite.laskupvm();

    for(auto rivi : rivit()) {
        QVariantMap map = rivi.toMap();

        int alvkoodi = map.value("alvkoodi").toInt();
        int alvprosentti = map.value("alvprosentti").toInt();

        if( alvkoodi >= Lasku::KAYTETYT) {
            alvkoodi = AlvKoodi::MYYNNIT_MARGINAALI;
            alvprosentti = 24;  // Marginaalilaskuilla käytössä vain 24% alv-prosentti
        } if( alvkoodi == AlvKoodi::MYYNNIT_NETTO && lasku.maksutapa() == Lasku::ENNAKKOLASKU) {
            alvkoodi = AlvKoodi::ENNAKKOLASKU_MYYNTI;
             map.insert("tili", kp()->asetukset()->luku("LaskuEnnakkotili"));
        } else if( alvkoodi == AlvKoodi::MYYNNIT_NETTO && kp()->onkoMaksuperusteinenAlv( pvm ) && lasku.maksutapa() != Lasku::KATEINEN)
            alvkoodi = AlvKoodi::MAKSUPERUSTEINEN_MYYNTI;                  

        bool vanhaan = false;

        if( !map.contains("ennakkohyvitys")) {
            for(int i=0; i < lista.count(); i++) {
                QVariantMap vmap = lista.at(i).toMap();

                if( map.value("tili") == vmap.value("tili") &&
                    map.value("kohdennus",0) == vmap.value("kohdennus",0) &&
                    map.value("alvprosentti",0) == vmap.value("alvprosentti",0) &&
                    alvkoodi == vmap.value("alvkoodi",0))
                {
                    double brutto = riviSumma( map);
                    double vanhasumma = vmap.value("kredit",0).toDouble() - vmap.value("debet",0).toDouble();
                    double uusisumma = vanhasumma + brutto;

                    if( uusisumma > 0.0) {
                        vmap.insert("kredit", uusisumma);
                        vmap.remove("debet");
                    } else {
                        vmap.insert("debet", 0 - uusisumma);
                        vmap.remove("kredit");
                    }
                    vmap.insert("tyyppi", TositeVienti::MYYNTI + TositeVienti::KIRJAUS);

                    lista[i] = vmap;
                    vanhaan = true;
                    break;
                }
            }
        }
        if( !vanhaan ) {
            TositeVienti vienti;
            vienti.setTili( map.value("tili").toInt() );
            vienti.setKohdennus( map.value("kohdennus").toInt());
            vienti.setAlvKoodi( alvkoodi );
            vienti.setAlvProsentti( alvprosentti);
            vienti.setPvm( pvm );
            if( tosite.kumppani()) {
                vienti.setKumppani( tosite.kumppani() ) ;
            }

            if( lasku.toimituspvm().isValid() )
                vienti.setJaksoalkaa( lasku.toimituspvm() );
            if( lasku.jaksopvm().isValid() )
                vienti.setJaksoloppuu( lasku.jaksopvm());

            double summa = riviSumma(map);
            if( summa > 0)
                vienti.setKredit( summa );
            else if( summa < 0)
                vienti.setDebet( 0 - summa);
            else
                continue;

            vienti.setSelite( tosite.otsikko() );
            if( alvkoodi == AlvKoodi::ENNAKKOLASKU_MYYNTI)
                vienti.setEra(-1);
            else if( map.contains("ennakkohyvitys"))
                vienti.setEra( map.value("ennakkohyvitys").toInt());

            lista.append(vienti);
        }
    }

    // Nyt vienteihin pitäisi vielä täydentään kotimaan alv:n alv-velkarivit

    QVariantList ulos;
    for(auto rivi: lista) {
        QVariantMap map = rivi.toMap();
        int alvkoodi = map.value("alvkoodi").toInt();
        if( alvkoodi == AlvKoodi::MYYNNIT_NETTO ||
            alvkoodi == AlvKoodi::ENNAKKOLASKU_MYYNTI ||
            alvkoodi == AlvKoodi::MAKSUPERUSTEINEN_MYYNTI) {
            double brutto = map.value("kredit",0).toDouble() - map.value("debet",0).toDouble();
            double netto = qRound64( brutto * 10000 / ( 100 + map.value("alvprosentti").toDouble() ) ) / 100.0;
            double vero = brutto - netto;

            if( netto > 0)
                map.insert("kredit", netto);
            else
                map.insert("debet", 0 - netto);

            ulos.append(map);

            TositeVienti verorivi;
            verorivi.setPvm( pvm );

            // TODO: Maksuperusteinen ALV ja Ennakkolaskutus
            if( alvkoodi == AlvKoodi::ENNAKKOLASKU_MYYNTI ) {
                verorivi.setTili( kp()->asetukset()->luku("LaskuEnnakkoALV") );
                verorivi.setAlvKoodi( AlvKoodi::ENNAKKOLASKU_MYYNTI + AlvKoodi::MAKSUPERUSTEINEN_KOHDENTAMATON );
                verorivi.setEra(-1);
            } else if( alvkoodi == AlvKoodi::MAKSUPERUSTEINEN_MYYNTI) {
                verorivi.setTili( kp()->tilit()->tiliTyypilla(TiliLaji::KOHDENTAMATONALVVELKA).numero());
                verorivi.setAlvKoodi( AlvKoodi::MAKSUPERUSTEINEN_MYYNTI + AlvKoodi::MAKSUPERUSTEINEN_KOHDENTAMATON);
                verorivi.setEra(-1);
            } else {
                verorivi.setTili( kp()->tilit()->tiliTyypilla(TiliLaji::ALVVELKA).numero() );
                verorivi.setAlvKoodi( AlvKoodi::MYYNNIT_NETTO + AlvKoodi::ALVKIRJAUS);
            }
            if( vero > 0)
                verorivi.setKredit(vero);
            else
                verorivi.setDebet(0 - vero);
            verorivi.setAlvProsentti( map.value("alvprosentti").toInt() );
            verorivi.setTyyppi( TositeVienti::ALVKIRJAUS + TositeVienti::MYYNTI );
            verorivi.setSelite( tosite.otsikko() + " ALV " + QString::number(map.value("alvprosentti").toInt()) );
            if( tosite.kumppani())
                verorivi.setKumppani(tosite.kumppani());
            ulos.append(verorivi);
        } else {
            ulos.append(map);
        }
    }
    return ulos;
    */
}

bool TositeRivit::onkoTyhja() const
{
    for( auto rivi : rivit_)
    {
        if( rivi.bruttoYhteensa().cents() || !rivi.nimike().isEmpty() )
            return false;
    }
    return true;
}

TositeRivi TositeRivit::rivi(int indeksi) const
{
    return rivit_.at(indeksi);
}

void TositeRivit::asetaRivi(int indeksi, const TositeRivi &rivi)
{
    rivit_[indeksi] = rivi;
    emit dataChanged( index(indeksi, NIMIKE),
                      index(indeksi, BRUTTOSUMMA));
}

void TositeRivit::lisaaRivi(TositeRivi rivi)
{
    if( !rivi.tili()) {
        int uusitili = kp()->asetukset()->luku("OletusMyyntitili",3000);
        rivi.setTili(uusitili);
        Tili *tili = kp()->tilit()->tili(uusitili);
        if(tili && kp()->asetukset()->onko(AsetusModel::AlvVelvollinen)) {
            rivi.setAlvKoodi( tili->alvlaji() );
            rivi.setAlvProsentti( tili->alvprosentti());
        }
    }

    beginInsertRows( QModelIndex(), rivit_.count(), rivit_.count() );
    rivit_.append(rivi);
    endInsertRows();
}

void TositeRivit::lisaaTuote(const Tuote &tuote, const QString &lkm)
{
    TositeRivi rivi;
    rivi.setTuote(tuote.id());
    rivi.setNimike(tuote.nimike());

    if( !tuote.unKoodi().isEmpty())
        rivi.setUNkoodi( tuote.unKoodi());
    else if( !tuote.yksikko().isEmpty())
        rivi.setYksikko( tuote.yksikko() );

    rivi.setLaskutetaanKpl( lkm );
    rivi.setMyyntiKpl( lkm.toDouble());

    rivi.setANetto( tuote.ahinta() );
    rivi.setTili( tuote.tili() );
    rivi.setKohdennus( tuote.kohdennus() );
    rivi.setAlvKoodi( tuote.alvkoodi() );
    rivi.setAleProsentti( tuote.alvprosentti() );

    rivi.laskeYhteensa();

    beginInsertRows( QModelIndex(), rivit_.count(), rivit_.count() );
    rivit_.append(rivi);
    endInsertRows();
}

/*    LaskuDialogi *dlg = qobject_cast<LaskuDialogi*>(parent());
    if (!rivi.isEmpty() && dlg && dlg->asiakkaanAlvTunnus().isEmpty() &&
            (rivi.value("alvkoodi").toInt() == AlvKoodi::YHTEISOMYYNTI_TAVARAT ||
             rivi.value("alvkoodi").toInt() == AlvKoodi::YHTEISOMYYNTI_PALVELUT ||
             rivi.value("alvkoodi").toInt() == AlvKoodi::RAKENNUSPALVELU_MYYNTI)) {

        QMessageBox::critical(dlg, tr("Käänteinen arvonlisävero"),
                              tr("Olet lisäämässä tuotetta, jolle sovelletaan käänteistä arvonlisäveroa. "
                                 "Käänteinen arvonlisävero on käytettävissä vain myytäessä yritykselle, "
                                 "jolla on alv-tunnus.\n\n"
                                 "Valitse ensin asiakas ja varmista, että asiakkaalle on tallennettu "
                                 "alv-tunnus."));

        return;
    }
*/    


void TositeRivit::poistaRivi(int rivi)
{
    beginRemoveRows(QModelIndex(), rivi, rivi);
    rivit_.removeAt(rivi);
    endRemoveRows();
}

void TositeRivit::asetaEnnakkolasku(bool ennakkoa)
{
    beginResetModel();
    ennakkolasku_ = ennakkoa;
    endResetModel();
}


