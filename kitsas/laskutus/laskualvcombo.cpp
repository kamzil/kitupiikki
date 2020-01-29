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
#include "laskualvcombo.h"

#include "db/verotyyppimodel.h"
#include "db/kirjanpito.h"

LaskuAlvCombo::LaskuAlvCombo(QWidget *parent, AsiakasVeroLaji asiakasverolaji) :
    QComboBox (parent)
{
    addItem(QIcon(":/pic/0pros.png"),"0%", QVariant(AlvKoodi::ALV0));
    addItem(QIcon(":/pic/netto.png"),"10%", QVariant(AlvKoodi::MYYNNIT_NETTO + 10 * 100 ));
    addItem(QIcon(":/pic/netto.png"),"14%", QVariant(AlvKoodi::MYYNNIT_NETTO + 14 * 100));
    addItem(QIcon(":/pic/netto.png"),"24%", QVariant(AlvKoodi::MYYNNIT_NETTO + 24 * 100 ));
    addItem(QIcon(":/pic/tyhja.png"),"Veroton", QVariant(AlvKoodi::EIALV ));

    if( asiakasverolaji == KAIKKI || asiakasverolaji == KOTIMAA)
        addItem(QIcon(":/pic/vasara.png"), tr("Rakennuspalvelut"), QVariant( AlvKoodi::RAKENNUSPALVELU_MYYNTI ));

    if( asiakasverolaji == EU) {
        addItem(QIcon(":/pic/eu.png"), tr("Tavaramyynti"), QVariant( AlvKoodi::YHTEISOMYYNTI_TAVARAT ));
        addItem(QIcon(":/pic/eu.png"), tr("Palvelumyynti"), QVariant( AlvKoodi::YHTEISOMYYNTI_PALVELUT ));
    }

/*    if( !kp()->onkoMaksuperusteinenAlv(kp()->paivamaara()))
    {
        addItem(QIcon(":/pic/marginaali.png"),"Voittomarginaalimenettely - käytetyt tavarat", QVariant(LaskuModel::Kaytetyt));
        addItem(QIcon(":/pic/marginaali.png"),"Voittomarginaalimenettely - taide-esineet", QVariant(LaskuModel::Taide));
        addItem(QIcon(":/pic/marginaali.png"),"Voittomarginaalimenettely - keräily- ja antiikkiesineet", QVariant(LaskuModel::KerailyAntiikki));
    }
*/
    if( kp()->asetukset()->onko("AlvVelvollinen") ) {
        setCurrentIndex(3); // Alv 24
    } else {
        setCurrentIndex(4);  // Veroton
    }
}

int LaskuAlvCombo::veroKoodi()
{   
    return currentData().toInt() % 100;
}

double LaskuAlvCombo::veroProsentti()
{
    if( veroKoodi() == AlvKoodi::MYYNNIT_NETTO)
        return currentData().toInt() / 100 * 1.0;
    else
        return 0.0;
}