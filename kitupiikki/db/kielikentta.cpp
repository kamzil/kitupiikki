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
#include "kielikentta.h"

#include <QDebug>

KieliKentta::KieliKentta()
{

}

KieliKentta::KieliKentta(const QVariant &var)
{
    aseta( var );
}

void KieliKentta::aseta(const QVariant &var)
{
    tekstit_.clear();
    if( !var.toMap().isEmpty() ) {
        QMapIterator<QString,QVariant> iter(var.toMap());
        while( iter.hasNext()) {
            iter.next();
            tekstit_.insert( iter.key(), iter.value().toString());
        }
    } else if(!var.toString().isEmpty()) {
        tekstit_.insert("fi", var.toString());
    }
}

QString KieliKentta::teksti(const QString &kieli) const
{
    if( tekstit_.contains(kieli))
        return tekstit_.value(kieli);
    if( tekstit_.contains("fi"))
        return tekstit_.value("fi");
    if( !tekstit_.isEmpty())
        return tekstit_.first();
    return QString();
}